# 📝 TODO - Compact Checklist

## 🔴 **CRITICAL (This Week)**

```
☐ Split widget .h files into .h + .cpp
  ├── diskvisualizationwindow.h → .h + .cpp
  ├── presetmanager.h → .h + .cpp
  └── trackgridwidget.h → .h + .cpp

☐ Create flux_core.h/c
  └── Define flux_disk_t, flux_track_t structures

☐ Implement SIMD MFM scalar
  └── src/core/uft_mfm_scalar.c (baseline)

☐ Fix .pro file
  ├── Add C11 flags
  ├── Add threading
  └── Add all sources
```

## 🟡 **IMPORTANT (Next Week)**

```
☐ Create DiskController
  └── src/controllers/diskcontroller.h/cpp

☐ Implement first tab class
  └── src/tabs/workflowtab.h/cpp

☐ Connect TrackGridWidget
  └── Real-time updates from controller

☐ Implement SSE2 decode
  └── src/core/uft_mfm_sse2.c (3-5x faster)
```

## 🟢 **NICE-TO-HAVE (Later)**

```
☐ Validation system
☐ Drive detection
☐ AVX2 decode (8-10x faster)
☐ Protection detection
☐ Batch operations
```

---

## 📂 **Files to Create:**

### **Immediately:**
```
include/uft/flux_core.h
src/core/flux_core.c
src/core/uft_mfm_scalar.c
src/widgets/diskvisualizationwindow.cpp
src/widgets/presetmanager.cpp
src/widgets/trackgridwidget.cpp
```

### **Soon:**
```
src/controllers/diskcontroller.h/cpp
src/tabs/workflowtab.h/cpp
src/core/uft_mfm_sse2.c
src/core/uft_gcr_scalar.c
```

---

## 🐛 **Fixes Needed:**

```
☐ uft_simd.c: Add #include <unistd.h>
☐ .pro: Add -lpthread
☐ .pro: Add -std=c11
☐ All .ui: Connect to tab classes
```

---

**Start here: Split widget files!**
