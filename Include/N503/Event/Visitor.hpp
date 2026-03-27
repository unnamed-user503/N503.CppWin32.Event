#pragma once

#include <N503/Event/Data.hpp>
#include <type_traits>
#include <utility>

namespace N503::Event
{
    /// @brief イベントツリーを走査し、メッセージを配送するクラス
    /// @details ターゲットとなるタグと配送データを保持し、配送の停止（Stop）制御を行います。
    /// @tparam Tag イベント配送に使用するタグの型
    template <typename Tag> 
    class Visitor
    {
    public:
        /// @brief デフォルトコンストラクタ
        Visitor() noexcept = default;

        /// @brief ターゲットタグとデータを指定して Visitor を構築します
        /// @param tag 配送対象となるノードの識別タグ
        /// @param data 配送するデータ
        template <typename T>
        requires (!std::is_reference_v<T>)
        explicit Visitor(Tag tag, T&& data) noexcept
            : m_Data(std::forward<T>(data))
            , m_Tag(tag)
        {
        }

        /// @brief 仮想デストラクタ
        virtual ~Visitor() = default;

        /// @brief イベントの配送を停止します
        /// @details 以降、この Visitor による子ノードや兄弟ノードへの伝搬が行われなくなります。
        auto Stop() const -> void
        {
            m_Stopped = true;
        }

        /// @brief 配送が停止されているかを確認します
        /// @return 停止されている場合は true
        [[nodiscard]]
        auto IsStopped() const -> bool
        {
            return m_Stopped;
        }

        /// @brief ターゲットタグを取得
        /// @return 識別タグ
        [[nodiscard]]
        auto GetTag() const -> Tag
        {
            return m_Tag;
        }

        /// @brief 格納されたデータを型安全に取得します
        /// @tparam T 取得する型
        /// @return T型のポインタ、または型が一致しなければ nullptr
        template <typename T>
        [[nodiscard]]
        auto As() const noexcept -> const T*
        {
            return m_Data.As<T>();
        }

        /// @brief データが格納されているかを確認します
        /// @return データが存在する場合は true
        [[nodiscard]]
        auto HasValue() const noexcept -> bool
        {
            return m_Data.HasValue();
        }

    private:
        /// @brief 配送するデータの格納
        Data m_Data;

        /// @brief 配送対象のタグ
        Tag m_Tag{};

        /// @brief 配送停止フラグ（const メソッド内から変更可能）
        mutable bool m_Stopped{false};
    };

} // namespace N503::Event
