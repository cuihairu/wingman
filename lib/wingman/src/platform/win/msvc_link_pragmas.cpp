// MSVC 自动链接指令薄层：#pragma comment(lib) 是 MSVC 翻译单元级指令，
// 不能进公共层（平台边界守卫禁 _WIN32 宏）。原散在 ml.cpp/ocr.cpp 的两条
// 指令收敛于此；档位条件与 CMakeLists 一致（WINGMAN_ENABLE_ML /
// WINGMAN_ENABLE_OCR 对应档位开启时对应 lib 才在链接路径），档位关闭
// 时不发指令，避免 LNK1104 找不到未安装的 lib。非 Windows 平台由 CMake
// target_link_libraries 提供同批库，本文件不参与编译（CMake win 源列表）。
#ifdef _WIN32
#ifdef WINGMAN_ENABLE_ML
#pragma comment(lib, "onnxruntime")
#endif
#ifdef WINGMAN_ENABLE_OCR
#pragma comment(lib, "tesseract51.lib")
#endif
#endif
