# 移动端搜索优惠券组件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在当前 Figma 文件中创建一个适用于移动端搜索结果页、包含未领取与已领取状态的迷你优惠券组件集。

**Architecture:** 使用一个 Component Set 管理两个同尺寸 Variant，以 `Status` 属性切换状态。每个 Variant 使用 Auto Layout 组织金额、门槛和状态操作区，通过统一尺寸与图层命名保证可复用性。

**Tech Stack:** Figma、Figma Plugin API、Auto Layout、Component Variants

## Global Constraints

- 每个 Variant 的尺寸必须为 156 × 44 px。
- 文案只包含 `¥20`、`满199可用` 与领取状态，不展示有效期。
- 视觉风格使用红橙促销风，并保留左右半圆缺口与中间短虚线。
- Component Set 的状态属性命名为 `Status`，取值为 `Unclaimed` 和 `Claimed`。

---

### Task 1: 创建并验收优惠券 Component Set

**Files:**
- Create: 当前 Figma 文件中的 `Coupon / Search Result Mini` Component Set

**Interfaces:**
- Consumes: 已确认的组件设计规格。
- Produces: 可通过 `Status=Unclaimed|Claimed` 切换的 Figma 组件集。

- [ ] **Step 1: 创建未领取 Variant 的结构**

  创建 156 × 44 px 的 Auto Layout 组件，加入 `¥20`、`满199可用`、`领取` 三段文案，并设置红橙渐变、高对比文字、左右半圆缺口和中间短虚线。

- [ ] **Step 2: 验证未领取 Variant**

  截图检查尺寸、文本完整性、间距、文字对比度和紧凑信息密度；预期文案不换行且无有效期。

- [ ] **Step 3: 创建已领取 Variant**

  复制未领取结构，保持 156 × 44 px，降低背景饱和度与对比度，并将操作文案改为 `已领取 ✓`。

- [ ] **Step 4: 组合并命名 Component Set**

  将两个组件组合为 `Coupon / Search Result Mini`，设置 Variant 属性 `Status=Unclaimed` 与 `Status=Claimed`。

- [ ] **Step 5: 最终视觉验收**

  对 Component Set 截图，核对两个状态尺寸一致、文字无溢出、状态差异清晰、无残留占位效果，并将组件集定位到当前视口。

- [ ] **Step 6: 提交计划文档**

  ```bash
  git add docs/superpowers/plans/2026-07-15-mobile-search-coupon-component.md
  git commit -m "docs: 添加移动搜索优惠券绘制计划" \
    -m "明确 Figma 组件结构、双状态创建步骤与视觉验收标准。" \
    -m "Prompt: 绘制移动端搜索结果页迷你优惠券组件。"
  ```
