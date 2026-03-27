#pragma once

#include <N503/Event/Node.hpp>
#include <algorithm>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace N503::Event
{

    /// @brief メモリ資源(Resource)を管理し、イベントノードを抽象的に生成するストレージ
    /// @tparam Tag イベントタグ
    /// @tparam Resource メモリ管理の実体 (Arena, Pool, Queue 等)
    template <typename Tag, typename Resource> class Storage final
    {
    public:
        /// @brief 核心：Resource の構築引数を転送する
        /// Arena(size), Queue(size, count) 等、デフォルトコンストラクタを持たない
        /// Resource を初期化リストで確実に実体化します。
        template <typename... Args>
        explicit Storage(Args&&... args)
            : m_Resource{ std::make_shared<Resource>(std::forward<Args>(args)...) }
        {
        }

        /// @brief コピーコンストラクタ（削除）
        Storage(const Storage&) = delete;

        /// @brief コピー代入演算子（削除）
        Storage& operator=(const Storage&) = delete;

        /// @brief ノードをメモリから生成し、shared_ptr で返す
        /// @tparam ConcreteNode ノードの型
        /// @param tag ノードのタグ
        /// @param handler イベントハンドラ
        /// @return 生成されたノードの shared_ptr、確保失敗時は nullptr
        template <typename ConcreteNode = Node<Tag>> auto Create(Tag tag, typename Node<Tag>::Handler handler = nullptr) -> std::shared_ptr<ConcreteNode>
        {
            // Resource からメモリを確保
            ConcreteNode* address = m_Resource->template Allocate<ConcreteNode>(1);

            if (!address)
            {
                return nullptr;
            }

            // オブジェクトの構築
            ConcreteNode* node = std::construct_at(address, tag, std::move(handler));

            // m_Resource(shared_ptr) を値キャプチャして延命させる
            auto resource = m_Resource;

            // shared_ptr のデリータで、この Storage の m_Resource に返却する
            auto deleter = [resource](ConcreteNode* pointer)
            {
                std::destroy_at(pointer);
                resource->Deallocate(pointer, 1);
            };

            return std::shared_ptr<ConcreteNode>(node, deleter);
        }

    private:
        /// @brief Arena, Pool, Queue 等のリソース実体
        std::shared_ptr<Resource> m_Resource;
    };

} // namespace N503::Event
