// EventData.hpp
#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeinfo>

namespace N503::Event
{

    /// @brief イベント型の要件
    /// @details 参照型（TDataType&）を許容せず、値またはポインタであることを保証します。
    template <typename TDataType>
    concept DataType = !std::is_reference_v<TDataType>;

    /// @brief 任意の型を保持するための型消去（Type Erasure）クラス
    /// @details SBO (Small Buffer Optimization) を備え、小さいデータはスタックバッファ、
    /// 大きなデータはヒープ（shared_ptr）に動的に割り当てて管理します。
    class Data
    {
    private:
        /// @brief スタック上のバッファサイズ
        /// 小さいイベント型（KeyboardEvent等、~48 bytes）はここに配置される
        static constexpr std::size_t BufferSize = 48;

    public:
        /// @brief 空の Data オブジェクトを構築します。
        Data() noexcept = default;

        /// @brief 任意の値を型消去して格納します。
        /// @tparam T 格納するデータの型
        /// @param value 格納する実体
        template <DataType TDataType> explicit Data(TDataType&& value) noexcept
        {
            using Decayed = std::decay_t<TDataType>;
            m_TypeInfo = &typeid(Decayed);

            if constexpr (sizeof(Decayed) <= BufferSize)
            {
                // SBO: バッファ内に直接構築し、破棄用の関数を登録
                new (m_Buffer.data()) Decayed(std::forward<TDataType>(value));
                m_Cleanup = [](void* p)
                {
                    std::destroy_at(static_cast<Decayed*>(p));
                };
            }
            else
            {
                // Heap: バッファに収まらない場合は shared_ptr で管理
                m_Heap = std::make_shared<Decayed>(std::forward<TDataType>(value));
            }
        }

        /// @brief コピーコンストラクタ（削除）
        /// @details イベントデータの二重管理を防ぐため禁止されています。
        Data(const Data&) = delete;

        /// @brief コピー代入演算子（削除）
        /// @return Data&
        auto operator=(const Data&) -> Data& = delete;

        /// @brief ムーブコンストラクタ（削除）
        /// @details 配信中にアドレスが変化することを防ぎ、安全性を担保するために禁止されています。
        /// @param other ムーブ元オブジェクト
        Data(Data&& other) = delete;

        /// @brief ムーブ代入演算子（削除）
        /// @return Data&
        auto operator=(Data&&) -> Data& = delete;

        /// @brief デストラクタ
        /// @details SBO 領域に構築されたオブジェクトのデストラクタを明示的に呼び出します。
        ~Data() noexcept
        {
            if (m_Cleanup)
            {
                m_Cleanup(m_Buffer.data());
            }
        }

        /// @brief 型安全な取得
        /// @tparam T 取得する型
        /// @return T型のポインタ、または型が一致しなければ nullptr
        template <DataType TDataType>
        [[nodiscard]]
        auto As() const noexcept -> const TDataType*
        {
            if (!m_TypeInfo || *m_TypeInfo != typeid(TDataType))
            {
                return nullptr;
            }

            // スタック格納 → バッファから取得
            if constexpr (sizeof(TDataType) <= BufferSize)
            {
                return reinterpret_cast<const TDataType*>(m_Buffer.data());
            }

            // ヒープ格納 → shared_ptr から取得
            if (m_Heap)
            {
                return reinterpret_cast<const TDataType*>(m_Heap.get());
            }

            return nullptr;
        }

        /// @brief 有効なデータが格納されているかを確認します。
        /// @return データが存在する場合は true
        [[nodiscard]]
        auto HasValue() const noexcept -> bool
        {
            return m_TypeInfo != nullptr;
        }

    private:
        /// @brief SBO用バッファ。アライメントを最大級に保証し、キャスト時の安全性を確保。
        alignas(std::max_align_t) std::array<std::byte, BufferSize> m_Buffer{};

        /// @brief 大きなデータ用のヒープ領域
        std::shared_ptr<void> m_Heap;

        /// @brief 格納されている型の実行時型情報
        const std::type_info* m_TypeInfo = nullptr;

        /// @brief バッファ内に構築されたオブジェクトを破棄するための関数ポインタ（型消去用）
        std::function<void(void*)> m_Cleanup;
    };

} // namespace N503::Event
