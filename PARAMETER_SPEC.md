# 3D Repeater - 最終パラメータ構造仕様

## 完全なパラメータ階層

```
ReptAll
├─ Copies X (int, default 8)
├─ Copies Y (int, default 1)
├─ Copies Z (int, default 1)
├─ Offset (float, default 0.0)
│
├─ Transform
│ ├─ Anchor Point (Point3D, default (0,0,0))
│ ├─ Position (Point3D, default (0,0,0))
│ ├─ Scale (Point3D %, default (100,100,100))
│ ├─ Rotation (Point3D deg, default (0,0,0))
│ ├─ Start Opacity (0–100, default 100)
│ ├─ End Opacity (0–100, default 100)
│ │
│ ├─ Step
│ │ ├─ Step Position (Point3D, default (80, 0, 0))
│ │ ├─ Step Rotation (Point3D deg, default (0, 0, 0))
│ │ └─ Step Scale (Point3D %, default (100, 100, 100))
│ │
│ └─ View
│ ├─ Projection (popup: Perspective / Ortho / None, default Perspective)
│ ├─ Perspective (0–100, default 35)
│ └─ Depth Scale (0–200, default 100)
│
└─ Random
├─ Master Seed (int, default 0)
├─ Master Strength (0–100, default 100)
├─ Distribution (popup: Uniform / Centered, default Centered)
│
├─ Fields
│ ├─ Spatial Field
│ │ ├─ Type (None / Box / Sphere, default None)
│ │ ├─ Center (Point3D, default (0,0,0))
│ │ ├─ Size (Point3D, default (2000,2000,2000))
│ │ ├─ Curve (Linear / EaseInOut, default EaseInOut)
│ │ └─ Invert (bool, default OFF)
│ │
│ ├─ Index Field
│ │ ├─ Type (None / Range, default None)
│ │ ├─ Start % (0–100, default 0)
│ │ ├─ End % (0–100, default 100)
│ │ └─ Curve (Linear / EaseInOut, default EaseInOut)
│ │
│ └─ Combine (popup: Multiply / Min / Max, default Multiply)
│
├─ Effector 1
│ ├─ Enable (bool, default ON)
│ ├─ Strength (0–100, default 100)
│ ├─ Seed Offset (int, default 0)
│ ├─ Probability (0–100, default 100)
│ ├─ Position Amount (Point3D, default (30, 0, 0))
│ ├─ Rotation Amount (Point3D deg, default (0, 20, 0))
│ ├─ Scale Amount (Point3D %, default (0,0,0))
│ └─ Opacity Amount (0–100, default 0)
│
├─ Effector 2
│ ├─ Enable (bool, default OFF)
│ ├─ Strength (0–100, default 100)
│ ├─ Seed Offset (int, default 100)
│ ├─ Probability (0–100, default 100)
│ ├─ Position Amount (Point3D, default (0,0,0))
│ ├─ Rotation Amount (Point3D deg, default (0,0,0))
│ ├─ Scale Amount (Point3D %, default (0,0,0))
│ └─ Opacity Amount (0–100, default 0)
│
└─ Effector 3
├─ Enable (bool, default OFF)
├─ Strength (0–100, default 100)
├─ Seed Offset (int, default 200)
├─ Probability (0–100, default 100)
├─ Position Amount (Point3D, default (0,0,0))
├─ Rotation Amount (Point3D deg, default (0,0,0))
├─ Scale Amount (Point3D %, default (0,0,0))
└─ Opacity Amount (0–100, default 0)
```

## 実装フェーズ

### Phase 1: 基本コピー機能 ✅ (現在実装中)
- Copies X
- Step Position X/Y

### Phase 2: 3D拡張
- Copies Y, Z
- Step Position Z
- Step Rotation
- Step Scale

### Phase 3: Transform基本
- Anchor Point
- Position
- Scale
- Rotation
- Start/End Opacity

### Phase 4: View/Projection
- Projection mode
- Perspective
- Depth Scale

### Phase 5: Random基本
- Master Seed
- Master Strength
- Distribution

### Phase 6: Fields
- Spatial Field
- Index Field
- Combine mode

### Phase 7: Effectors
- Effector 1, 2, 3
- Enable/Strength/Seed
- Position/Rotation/Scale/Opacity amounts

## 技術的制約

- After Effectsの最大パラメータ数制限を考慮
- UIグループ化のためのトピック使用
- パフォーマンス最適化が必要
