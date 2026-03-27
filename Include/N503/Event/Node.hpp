#pragma once

#ifndef N503_EVENT_REGISTRY_ARRAY_ENABLED
#define N503_EVENT_REGISTRY_ARRAY_ENABLED
#endif

#include <N503/Event/Details/StatisticsPolicy.hpp>
#include <N503/Event/Details/ValidationPolicy.hpp>
#include <N503/Event/Visitor.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>

namespace N503::Event
{
    /// @brief イベント配信におけるノードの有効状態を定義する列挙型
    enum class State
    {
        Active,    ///< 有効状態：自身が反応し、子への配信も行う
        Passive,   ///< 透過状態：自身は反応しない（配送はノードの実装に依存）
        Destroyed, ///< 破棄状態：次回のクリーンアップ（Sweep）対象となる
    };

    /// @brief イベント通知システムのノードクラス
    /// @tparam Tag イベントタグの型
    /// @tparam MaxTags 配列使用時の最大タグ数
    template <typename Tag, std::size_t MaxTags = 64>
    class Node final
        : public std::enable_shared_from_this<Node<Tag, MaxTags>>
        , protected Details::StatisticsPolicy<Tag, MaxTags>
        , protected Details::ValidationPolicy<Tag, MaxTags>
    {
    public:
        /// @brief イベントハンドラの型定義
        using Handler = std::function<void(const Visitor<Tag>&)>;

        /// @brief 特定のタグを持つ子孫ノードの総数を取得します
        /// @param tag 検索するタグ
        /// @return 子孫に含まれる対象タグの数
        using Details::StatisticsPolicy<Tag, MaxTags>::GetTagCount;

        /// @brief コンストラクタ
        /// @param tag このノードに割り当てるタグ
        /// @param handler このノードがイベントを受信した際の処理
        explicit Node(Tag tag, Handler handler = nullptr)
            : m_Tag(tag)
            , m_Handler(std::move(handler))
            , m_State(State::Active)
        {
        }

        /// @brief 子ノードを追加し、統計情報を親方向に更新します
        /// @param child 追加する子ノードの共有ポインタ
        /// @throws std::logic_error 子がすでに親を持つ、自己参照、または循環参照の場合
        auto AddChild(std::shared_ptr<Node> child) -> void
        {
            if (!child)
            {
                return;
            }

            // チェック1：子がすでに親を持っているか
            if (!child->m_Parent.expired())
            {
                throw std::logic_error("Child node already has a parent. Remove it from its current parent first.");
            }

            // チェック2：自分自身を子にしようとしていないか
            if (child.get() == this)
            {
                throw std::logic_error("Cannot add node as its own child (self-reference).");
            }

            // チェック3：子の子孫が自分を含んでいないか（循環参照防止）
            if (this->IsDescendantOf(child.get(), this))
            {
                throw std::logic_error("Cannot add ancestor as child (circular reference).");
            }

            child->m_Parent = this->shared_from_this();
            this->AddChildStats(child.get());
            m_Children.push_back(std::move(child));
        }

        /// @brief 子ノードを削除し、統計情報を親方向に更新します
        /// @param child 削除する子ノードの共有ポインタ
        /// @return 削除された子ノード（orphan 状態）
        /// @throws std::logic_error null ポインタ、Destroyed 状態、または親が一致しない場合
        auto RemoveChild(std::shared_ptr<Node> child) -> std::shared_ptr<Node>
        {
            if (!child)
            {
                throw std::logic_error("Cannot remove null child");
            }

            // Destroyed なら Sweep に任せるべき
            if (child->m_State == State::Destroyed)
            {
                throw std::logic_error(
                    "Cannot RemoveChild: child is already marked Destroyed. "
                    "Call registry.Update() or use Handle instead."
                );
            }

            // 親が一致するか確認
            if (child->m_Parent.lock().get() != this)
            {
                throw std::logic_error(
                    "Child's parent does not match this node. "
                    "Use the correct parent node to remove."
                );
            }

            // 子リストから検索
            auto iterate = std::find(m_Children.begin(), m_Children.end(), child);
            if (iterate == m_Children.end())
            {
                throw std::logic_error(
                    "Child not found in m_Children list. "
                    "Possible data corruption (parent reference exists but not in list)."
                );
            }

            // デバッグ：二重登録をチェック
#ifdef _DEBUG
            if (std::count(m_Children.begin(), m_Children.end(), child) != 1)
            {
                throw std::logic_error(
                    "Child is registered multiple times in m_Children. "
                    "Serious data corruption detected."
                );
            }
#endif

            // 統計ロールバック
            this->RemoveChildStats(child.get());

            // 親参照をクリア
            child->m_Parent.reset();

            // 子リストから削除
            m_Children.erase(iterate);

            return child;
        }

        /// @brief イベントを受け入れ、自身および子孫ノードへ通知を伝播させます
        /// @param visitor イベント情報とトラバーサル制御を持つビジター
        auto Accept(const Visitor<Tag>& visitor) -> void
        {
            if (m_State == State::Destroyed)
            {
                return;
            }

            // イベントの伝搬を停止しているのにここでチェックしないとNotify()が呼ばれてしまう。
            if (visitor.IsStopped())
            {
                return;
            }

            // 枝切り最適化：自身がターゲットか、子孫にターゲットがいる場合のみ続行
            if (m_Tag != visitor.GetTag() && GetTagCount(visitor.GetTag()) == 0)
            {
                return;
            }

            // 自身が Active なら通知
            if (m_State == State::Active)
            {
                Notify(visitor);
            }

            // Notify()で通知した先でStop()を呼んだ場合はここで伝搬させるのを止める
            if (visitor.IsStopped())
            {
                return;
            }

            // 子孫へ配送
            for (auto& child : m_Children)
            {
                // 兄弟の誰かがStop()を呼んだ場合は他の兄弟へイベントを伝搬するのを止める
                if (visitor.IsStopped())
                {
                    break;
                }

                child->Accept(visitor);
            }
        }

        /// @brief 破棄フラグが立った子ノードを物理削除し、統計情報を同期します
        auto Sweep() -> void
        {
            auto iterate = m_Children.begin();
            while (iterate != m_Children.end())
            {
                const auto& child = *iterate;

                // 子の子孫も再帰的に Sweep
                child->Sweep();

                // Destroyed なら物理削除
                if (child->m_State == State::Destroyed)
                {
                    this->RemoveChildStats(child.get());
                    iterate = m_Children.erase(iterate);
                }
                else
                {
                    ++iterate;
                }
            }
        }

        /// @brief 条件が一致する場合、登録されたハンドラを実行します
        /// @param visitor イベントビジター
        auto Notify(const Visitor<Tag>& visitor) const -> void
        {
            if (m_Tag == visitor.GetTag() && m_Handler)
            {
                m_Handler(visitor);
            }
        }

        /// @brief ノードの状態を設定します
        /// @param state 設定する状態（Active/Passive/Destroyed）
        auto SetState(State state) noexcept -> void
        {
            m_State = state;
        }

        /// @brief ノードの現在の状態を取得します
        /// @return 現在の State
        [[nodiscard]]
        auto GetState() const noexcept -> State
        {
            return m_State;
        }

        /// @brief ノードに設定されたタグを取得します
        /// @return このノードのタグ
        [[nodiscard]]
        auto GetTag() const noexcept -> Tag
        {
            return m_Tag;
        }

    private:
        friend class Details::StatisticsPolicy<Tag, MaxTags>;
        friend class Details::ValidationPolicy<Tag, MaxTags>;

        /// @brief このノードの識別タグ
        Tag m_Tag;

        /// @brief イベント発生時に実行されるコールバック
        Handler m_Handler;

        /// @brief ノードの生存状態
        State m_State;

        /// @brief 親ノードへの弱参照（循環参照防止）
        std::weak_ptr<Node> m_Parent;

        /// @brief 所有している子ノードのリスト
        std::list<std::shared_ptr<Node>> m_Children;
    };

} // namespace N503::Event
