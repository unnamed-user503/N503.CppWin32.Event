#pragma once

#ifndef N503_EVENT_REGISTRY_ARRAY_ENABLED
#define N503_EVENT_REGISTRY_ARRAY_ENABLED
#endif

#ifdef N503_EVENT_REGISTRY_ARRAY_ENABLED
#include <array>
#else
#include <unordered_map>
#endif

#include <cstddef>
#include <stdexcept>

namespace N503::Event
{
    // 前方宣言
    template <typename TTag, std::size_t TMaxTags>
    class Node;
}

namespace N503::Event::Details
{

    /// @brief ノードの統計情報管理と更新を担当するポリシー
    /// @tparam Tag イベントタグの型
    /// @tparam MaxTags 配列使用時の最大タグ数
    template <typename TTag, std::size_t TMaxTags = 64>
    class StatisticsPolicy
    {
    protected:
        /// @brief 
        StatisticsPolicy()
        {
#ifdef N503_EVENT_REGISTRY_ARRAY_ENABLED
            m_TagCounts.fill(0);
#else
            m_TagCounts.clear();
#endif
        }

        /// @brief 子ノード追加時の統計更新
        /// @param child 追加されたノード
        auto AddChildStats(const Node<TTag, TMaxTags>* child) -> void
        {
            this->UpdateStats(child, 1);
        }

        /// @brief 子ノード削除時の統計更新
        /// @param child 削除されたノード
        auto RemoveChildStats(const Node<TTag, TMaxTags>* child) -> void
        {
            this->UpdateStats(child, -1);
        }

        /// @brief 特定のタグを持つ子孫ノードの総数を取得
        /// @param tag 検索するタグ
        /// @return 子孫に含まれる対象タグの数
        [[nodiscard]]
        auto GetTagCount(TTag tag) const noexcept -> std::size_t
        {
#ifdef N503_EVENT_REGISTRY_ARRAY_ENABLED
            const auto index = static_cast<std::size_t>(tag);
            if (index >= TMaxTags)
            {
                return 0;
            }
            return m_TagCounts[index];
#else
            auto it = m_TagCounts.find(tag);
            return (it != m_TagCounts.end()) ? it->second : 0;
#endif
        }

    private:
        /// @brief 統計情報の一括更新（内部用）
        /// @details 子の「自身のタグ(+1/-1)」と、子が既に持っている「子孫の全統計」を
        ///          ルートまで一度の走査で一気に加算する
        /// @param child 追加/削除されたノード
        /// @param delta 適用する変化（+1 または -1）
        /// @param current 更新開始ノード（通常は this）
        auto UpdateStats(const Node<TTag, TMaxTags>* child, int delta) -> void
        {
            auto current = static_cast<Node<TTag, TMaxTags>*>(this);

            while (current)
            {
                // 1. 子自身のタグ分を更新
#ifdef N503_EVENT_REGISTRY_ARRAY_ENABLED
                current->m_TagCounts[static_cast<std::size_t>(child->m_Tag)] += delta;
#else
                current->m_TagCounts[child->m_Tag] += delta;
#endif

                // 2. 子が持っていた全子孫の統計分を更新
#ifdef N503_EVENT_REGISTRY_ARRAY_ENABLED
                for (std::size_t i = 0; i < TMaxTags; ++i)
                {
                    if (child->m_TagCounts[i] > 0)
                    {
                        current->m_TagCounts[i] += (static_cast<int>(child->m_TagCounts[i]) * delta);
                    }
                }
#else
                for (const auto& [tag, count] : child->m_TagCounts)
                {
                    if (count > 0)
                    {
                        current->m_TagCounts[tag] += (static_cast<int>(count) * delta);
                    }
                }
#endif

                // ルートまで遡る
                if (auto parent = current->m_Parent.lock())
                {
                    current = parent.get();
                }
                else
                {
                    break;
                }
            }
        }

    protected:
        /// @brief 子孫ノードのタグ別合計カウント
#ifdef N503_EVENT_REGISTRY_ARRAY_ENABLED
        std::array<std::size_t, TMaxTags> m_TagCounts;
#else
        std::unordered_map<TTag, std::size_t> m_TagCounts;
#endif
    };

} // namespace N503::Event::Details
