#pragma once

#include <N503/Event/Node.hpp>
#include <N503/Event/Storage.hpp>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

namespace N503::Event
{

    /// @brief イベントタグが満たすべき要件
    template <typename T>
    concept EventTag = requires(T tag)
    {
        /// @brief タグが整数型に変換可能（配列インデックス用）
        { static_cast<std::size_t>(tag) } -> std::convertible_to<std::size_t>;

        /// @brief タグが比較可能
        { tag == tag } -> std::convertible_to<bool>;
    };

    /// @brief リソースアロケータが満たすべき要件
    template <typename Resource, typename NodeType>
    concept ResourceAllocator = requires(Resource& resource, NodeType* pointer, std::size_t count)
    {
        /// @brief 型 T のメモリを count 個分確保
        { resource.template Allocate<NodeType>(count) } -> std::convertible_to<NodeType*>;

        /// @brief メモリを返却
        { resource.Deallocate(pointer, count) } -> std::same_as<void>;

        /// @brief 型付きアロケータ(Pool)の場合は ValueType が一致する必要がある
        requires (!requires { typename Resource::ValueType; } || std::is_same_v<typename Resource::ValueType, NodeType>);
    };

    /// @brief イベントシステムのエントリポイント。
    /// メモリ管理(Storage)とツリーのルートをラップし、簡潔な操作を提供します。
    /// @warning Registry の生存期間は、すべての Node より長くする必要があります。
    /// Node が存在する間に Registry を破棄すると、クラッシュが発生します。
    template <typename Tag, typename Resource> requires ResourceAllocator<Resource, Node<Tag>> class Registry final
    {
    public:
        /// @brief コンストラクタ。内部 Resource の初期化引数を転送します。
        template <typename... Args>
        explicit Registry(Tag tag, Args&&... args)
            : m_Storage(std::forward<Args>(args)...)
        {
            m_Root = m_Storage.template Create<Node<Tag>>(tag);
        }

        /// @brief 指定した親ノードに子要素を追加する (形式: Registry::AddChild(parent, tag, handler))
        /// @param parent 親となる Node ノード
        /// @param tag 新しいノードのタグ
        /// @param handler 実行されるコールバック
        /// @return 生成された子ノードの共有ポインタ
        auto AddChild(const std::shared_ptr<Node<Tag>>& parent, Tag tag, typename Node<Tag>::Handler handler = nullptr) -> std::shared_ptr<Node<Tag>>
        {
            if (!parent)
            {
                return nullptr;
            }

            // Storage を使用してノードを生成
            auto child = m_Storage.template Create<Node<Tag>>(tag, std::move(handler));

            if (!child)
            {
                throw std::bad_alloc();
            }

            // Node::AddChild(shared_ptr<Node>) を呼び出し、内部リストへ登録・統計更新
            parent->AddChild(child);

            return child;
        }

        /// @brief ルート直下に子要素を追加する簡略形式
        auto AddChild(Tag tag, typename Node<Tag>::Handler handler = nullptr) -> std::shared_ptr<Node<Tag>>
        {
            return AddChild(m_Root, tag, std::move(handler));
        }

        /// @brief イベントを受け入れ、自身および子孫ノードへ通知を伝播させます
        /// @param visitor イベント情報とトラバーサル制御を持つビジター
        auto Accept(const Visitor<Tag>& visitor) -> void
        {
            m_Root->Accept(visitor);
        }

        /// @brief 不要になったノード（State::Destroyed）の物理削除と統計のクリーンアップを実行
        void Update()
        {
            if (m_Root)
            {
                m_Root->Sweep(); // Node.hpp の Sweep ロジックを呼び出し
            }
        }

        /// @brief ルートノードを取得
        auto GetRoot() const -> std::shared_ptr<Node<Tag>>
        {
            return m_Root;
        }

    private:
        /// @brief メモリ資源と生成ロジックの管理
        Storage<Tag, Resource> m_Storage;

        /// @brief イベントツリーの起点
        std::shared_ptr<Node<Tag>> m_Root;
    };

} // namespace N503::Event
