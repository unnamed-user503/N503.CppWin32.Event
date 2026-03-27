# N503 Event System — 使い方ガイド

イベントツリーを構築し、タグに基づいてイベントを配送するための軽量フレームワーク。  
メモリ管理（Arena / Pool / Stack / Queue）と組み合わせて高速に動作します。

---

## 1. Data — イベントデータの型消去（SBO 対応）

### 役割
- 任意の型をイベントデータとして保持する
- 小さいデータは SBO（Small Buffer Optimization）でスタック領域に格納
- 大きいデータは shared_ptr でヒープ管理
- 型安全に取り出せる

### 使用例
```cpp
Data data{ KeyboardEvent{42} };

if (auto* keyboardEvent = data.As<KeyboardEvent>())
{
    // keyboardEvent にアクセスできる
}
```

---

## 2. Visitor — イベント配送のコンテナ

### 役割
- ターゲットタグを保持
- Data を保持
- Stop() による配送停止が可能

### 使用例
```cpp
Visitor<Tag> visitor{ Tag::Click, ClickEvent{100, 200} };
visitor.Stop(); // 配送停止
```

---

## 3. Node — イベントツリーの構成要素

### 役割
- 親子関係を持つツリー構造
- タグを持つ
- ハンドラを持つ
- 子孫のタグ統計（StatisticsPolicy）
- 循環参照チェック（ValidationPolicy）
- Accept(visitor) でイベント配送

### 子ノード追加
```cpp
auto child = registry.AddChild(parent, Tag::Click, [](const Visitor<Tag>& visitor)
{
    if (auto* data = visitor.As<ClickEvent>())
    {
        // クリックイベント処理
    }
});
```

### イベント配送
```cpp
Visitor<Tag> visitor{ Tag::Click, ClickEvent{10, 20} };
root->Accept(visitor);
```

---

## 4. Handle — RAII によるノード破棄

### 役割
- Handle が破棄されると対象ノードを State::Destroyed に変更
- 次回 Registry::Update() で安全にツリーから除去される

### 使用例
```cpp
Handle<Tag> handler{ registry.AddChild(root, Tag::Click) };
// スコープを抜けると Destroyed 状態になる
```

---

## 5. Registry — イベントシステムのエントリポイント

### 役割
- ルートノードの生成
- 子ノードの生成
- Update() による Destroyed ノードの掃除
- Storage によるメモリ管理

### 使用例
```cpp
enum class Tag { Root, Click, Hover };

Registry<Tag, Pool<Node<Tag>>> registry{ Tag::Root, 128 };

auto clickNode = registry.AddChild(Tag::Click, [](const Visitor<Tag>& visitor)
{
    if (auto* data = visitor.As<ClickEvent>())
    {
        // 処理
    }
});

Visitor<Tag> visitor{ Tag::Click, ClickEvent{1, 2} };

registry.Accept(visitor);
registry.Update(); // Destroyed ノードの掃除
```

---

## 6. Storage — メモリ管理と Node 生成の抽象化

### 役割
- Pool / Arena / Stack / Queue を透過的に扱う
- shared_ptr のカスタムデリータで自動返却
- Node 側はアロケータを意識しない

### 使用例（Pool）
```cpp
Storage<Tag, Pool<Node<Tag>>> storage{ 128 };
auto node = storage.Create<Node<Tag>>(Tag::Click, handler);

```
---

## 7. 設計上のポイント

- Node は shared_ptr で管理される  
- 親は weak_ptr（循環参照防止）  
- Handle による安全な破棄  
- StatisticsPolicy によるタグ統計で高速枝切り  
- ValidationPolicy による循環参照防止  
- Storage によるアロケータ抽象化  


---

## 8. テストコード

```cpp
#include <iostream>
#include <cassert>

#include <N503/Event/Registry.hpp>
#include <N503/Event/Node.hpp>
#include <N503/Event/Visitor.hpp>
#include <N503/Event/Handle.hpp>
#include <N503/Event/Data.hpp>
#include <N503/Event/Storage.hpp>

#include <N503/Memory.hpp>

// -----------------------------
// テスト用のイベントデータ
// -----------------------------
struct ClickEvent
{
    int x;
    int y;
};

// -----------------------------
// タグ定義
// -----------------------------
enum class Tag
{
    Root,
    Click,
    Hover
};

// -----------------------------
// テスト開始
// -----------------------------
int main()
{
    using namespace N503::Event;

    // -----------------------------------------
    // 1. Registry の構築（Root を自動生成）
    // -----------------------------------------
    Registry<Tag, N503::Memory::Storage::Pool<Node<Tag>>> registry{ Tag::Root, 128 };

    auto root = registry.GetRoot();
    assert(root);
    assert(root->GetTag() == Tag::Root);

    std::cout << "[OK] Registry root created\n";

    // -----------------------------------------
    // 2. 子ノード追加テスト
    // -----------------------------------------
    bool clickHandled = false;

    auto clickNode = registry.AddChild(Tag::Click, [&](const Visitor<Tag>& v)
    {
        if (auto* ev = v.As<ClickEvent>())
        {
            clickHandled = true;
            std::cout << "[OK] ClickEvent handled: (" << ev->x << ", " << ev->y << ")\n";
        }
    });

    assert(clickNode);
    assert(clickNode->GetTag() == Tag::Click);

    std::cout << "[OK] Child node added\n";

    // -----------------------------------------
    // 3. イベント配送テスト
    // -----------------------------------------
    Visitor<Tag> visitor{ Tag::Click, ClickEvent{ 10, 20 } };
    registry.Accept(visitor);

    assert(clickHandled == true);

    std::cout << "[OK] Event delivered to Click node\n";

    // -----------------------------------------
    // 4. Stop() の動作確認
    // -----------------------------------------
    bool secondHandled = false;

    auto first = registry.AddChild(Tag::Click, [&](const Visitor<Tag>& v)
    {
        if (auto* ev = v.As<ClickEvent>())
        {
            v.Stop();
        }
    });

    auto second = registry.AddChild(Tag::Click, [&](const Visitor<Tag>& v)
    {
        secondHandled = true;
    });

    Visitor<Tag> stopVisitor{ Tag::Click, ClickEvent{1, 1} };
    registry.Accept(stopVisitor);

    assert(secondHandled == false);

    std::cout << "[OK] Stop() prevents further propagation\n";

    // -----------------------------------------
    // 5. Handle による Destroyed ノードの破棄
    // -----------------------------------------
    auto tempNode = registry.AddChild(Tag::Hover);
    {
        Handle<Tag> handle{ tempNode };
        // スコープを抜けると Destroyed に設定される
    }

    assert(tempNode->GetState() == State::Destroyed);

    std::cout << "[OK] Handle destroyed node marked as Destroyed\n";

    // -----------------------------------------
    // 6. Update() による物理削除
    // -----------------------------------------
    registry.Update();

    // Hover の子孫数が 0 なら削除成功
    assert(root->GetTagCount(Tag::Hover) == 0);

    std::cout << "[OK] Destroyed node physically removed by Update()\n";

    // -----------------------------------------
    // 7. Data の SBO / Heap 動作確認
    // -----------------------------------------
    {
        Data small{ ClickEvent{1, 2} };
        assert(small.As<ClickEvent>() != nullptr);
        std::cout << "[OK] Data SBO works\n";
    }

    {
        struct Big { char buf[256]; };
        Data big{ Big{} };
        assert(big.As<Big>() != nullptr);
        std::cout << "[OK] Data heap allocation works\n";
    }

    std::cout << "\nAll tests passed successfully.\n";
    return 0;
}
```
