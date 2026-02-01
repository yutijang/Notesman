## Notesman

<details>
<summary>IMPORTANT NOTICE</summary>

**THÔNG BÁO QUAN TRỌNG: DỰ ÁN CÁ NHÂN VÀ HỌC TẬP**

Dự án này là một kho lưu trữ mã nguồn cá nhân được tạo ra **chỉ nhằm mục đích học tập và thực hành** kỹ năng C++ hiện đại (Qt6, OOP, Best Practice,...).

**CHÍNH SÁCH ĐÓNG GÓP:**

* **Dự án này KHÔNG chấp nhận bất kỳ hình thức đóng góp nào.**
* **Vui lòng KHÔNG gửi Pull Request (PRs).**
* **Vui lòng KHÔNG tạo Issues (Báo cáo lỗi, đề xuất tính năng, hoặc câu hỏi).**

Kho lưu trữ này được duy trì riêng bởi chủ sở hữu để theo dõi quá trình phát triển cá nhân. Mọi Issues/PRs được tạo sẽ bị đóng ngay lập tức mà không cần phản hồi. Cảm ơn sự thông cảm của bạn.

---

**IMPORTANT NOTICE: PERSONAL AND LEARNING PROJECT**

This project is a personal source code repository created solely **for the purpose of learning and practicing** modern C++ skills (Qt6, OOP, Best Practices, etc.).

**CONTRIBUTION POLICY:**

* **This project does NOT accept any form of contributions.**
* **Please DO NOT send Pull Requests (PRs).**
* **Please DO NOT create Issues (bug reports, feature requests, or questions).**

This repository is maintained solely by the owner to track personal development progress. Any created Issues/PRs will be closed immediately without response. Thank you for your understanding.
</details>

## Features
In TO-DO List

## Prerequisites
In TO-DO List

## Building & Running
In TO-DO List

## Usage
In TO-DO List

## License
[![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/yutijang/Notesman?tab=License-1-ov-file)

## Qt Source (LGPLv3)
**Note on Qt6 Libraries (LGPLv3 Compliance):** This application links to Qt6 dynamically. To use a custom-built version of the Qt libraries:
1. Build your custom Qt6 from source using the same toolchain (e.g., `clang-cl` or `clang++`).
2. Re-configure the project by setting `CMAKE_PREFIX_PATH` to your custom installation:
```yaml
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/custom/qt
cmake --build build
```
3. Alternatively, for a quick test, replace the existing Qt dynamic libraries (`.dll` or `.so`) in the application's executable directory with your custom ones.