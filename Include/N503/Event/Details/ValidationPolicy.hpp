#pragma once

#include <cstddef>
#include <stdexcept>

namespace N503::Event
{
    // 前方宣言
    template <typename TTag, std::size_t MaxTags> class Node;
} // namespace N503::Event

namespace N503::Event::Details
{

    /// @brief ノードの親子関係における検証を担当するポリシー
    /// @tparam TTag イベントタグの型
    /// @tparam TMaxTags 配列使用時の最大タグ数
    template <typename TTag, std::size_t TMaxTags = 64> class ValidationPolicy
    {
    protected:
        /// @brief 指定ノードが別ノードの子孫であるかを判定
        /// @details descendant をルートまで遡り、ancestor に到達するかを確認する
        /// @param descendant 子孫候補のノード
        /// @param ancestor 祖先候補のノード
        /// @return ancestor が descendant の祖先なら true
        auto IsDescendantOf(const Node<TTag, TMaxTags>* descendant, const Node<TTag, TMaxTags>* ancestor) -> bool
        {
            if (!descendant || !ancestor)
            {
                return false;
            }

            // descendant の親をたどっていき、ancestor に到達するか確認
            Node<TTag, TMaxTags>* current = descendant->m_Parent.lock().get();

            while (current != nullptr)
            {
                if (current == ancestor)
                {
                    return true; // 祖先が見つかった
                }

                current = current->m_Parent.lock().get();
            }

            return false; // 祖先が見つからなかった
        }
    };

} // namespace N503::Event::Details
