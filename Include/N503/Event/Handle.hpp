#pragma once

#include <N503/Event/Node.hpp>

#include <algorithm>
#include <memory>
#include <utility>

namespace N503::Event
{

    /// @brief ノードの生存期間と状態を管理するRAIIハンドル
    /// @details ハンドルが破棄される際、管理対象ノードの状態を自動的に State::Destroyed に変更します。
    /// これにより、Registry::Update() 実行時に安全にツリーから切り離されます。
    /// @tparam Tag イベント配送に使用するタグの型
    template <typename Tag> class Handle final
    {
    public:
        /// @brief 管理対象となるノードを指定してハンドルを構築します。
        /// @param node 管理対象のノードポインタ
        explicit Handle(std::shared_ptr<Node<Tag>> node)
            : m_Node(std::move(node))
        {
        }

        /// @brief コピーコンストラクタ（削除）
        /// @details 二重管理による意図しない状態変更を防ぐため禁止されています。
        Handle(const Handle&) = delete;

        /// @brief コピー代入演算子（削除）
        /// @return Handle&
        Handle& operator=(const Handle&) = delete;

        /// @brief ムーブコンストラクタ
        /// @param other ムーブ元オブジェクト
        Handle(Handle&&) noexcept = default;

        /// @brief ムーブ代入演算子
        /// @param other ムーブ元オブジェクト
        /// @return Handle&
        Handle& operator=(Handle&&) noexcept = default;

        /// @brief デストラクタ
        /// @details 管理しているノードが存在する場合、Reset() を呼び出してノードを破棄状態にします。
        ~Handle()
        {
            Reset();
        }

        /// @brief 管理対象ノードのメンバにアクセスするためのアロー演算子
        /// @return ノードの生ポインタ
        auto operator->() const
        {
            return m_Node.get();
        }

        /// @brief 管理対象ノードの共有ポインタを取得します。
        /// @return ノードの共有ポインタ
        auto GetNode() const -> std::shared_ptr<Node<Tag>>
        {
            return m_Node;
        }

        /// @brief 管理しているノードを明示的に破棄状態にし、参照を解放します。
        /// @details ノードが存在する場合、その状態を State::Destroyed に設定したあと、
        /// 内部で保持している shared_ptr をリセットします。
        auto Reset() -> void
        {
            if (m_Node)
            {
                m_Node->SetState(State::Destroyed);
                m_Node.reset(); // 明示的に参照を離す
            }
        }

    private:
        /// @brief 管理対象のノード
        std::shared_ptr<Node<Tag>> m_Node;
    };

} // namespace N503::Event
