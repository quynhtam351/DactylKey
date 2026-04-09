#!/usr/bin/env python3
"""
Tạo prompt từ source code dự án bàn phím Dactyl với STM32.

Cách dùng:
    python generate_prompt.py
"""

import os
import sys
import re
from pathlib import Path
from datetime import datetime


# ============================================================
# CẤU HÌNH CỐ ĐỊNH
# ============================================================

SCAN_DIRS = [
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\Core\Src",
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\Core\Inc",
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\USB_DEVICE",
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\Application",
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\Config",
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\Protocol",
    r"C:\Code\Keyboard_Blackpill\Dactyl_Left\Middleware"
]

EXTENSIONS = {'.c', '.h'}
PROJECT_DESCRIPTION = "Dự án bàn phím Dactyl với STM32"
MAX_FILE_SIZE_KB = 200


# ============================================================
# XỬ LÝ COMMENT
# ============================================================

def remove_comments(code: str) -> str:
    """Loại bỏ tất cả comment trong code C/C++."""
    # Loại bỏ /* ... */
    code = re.sub(r'/\*[\s\S]*?\*/', '', code)
    
    # Loại bỏ //
    code = re.sub(r'//.*?$', '', code, flags=re.MULTILINE)
    
    # Loại bỏ dòng trống liên tiếp
    code = re.sub(r'\n\s*\n\s*\n', '\n\n', code)
    
    # Loại bỏ khoảng trắng thừa cuối dòng
    code = re.sub(r'[ \t]+$', '', code, flags=re.MULTILINE)
    
    return code.strip()


# ============================================================
# QUÉT FILE
# ============================================================

def is_binary(file_path: Path) -> bool:
    """Kiểm tra file có phải binary không."""
    try:
        with open(file_path, 'rb') as f:
            chunk = f.read(1024)
            return b'\x00' in chunk
    except Exception:
        return True


def scan_directories() -> list:
    """
    Quét toàn bộ các thư mục trong SCAN_DIRS (bao gồm cả thư mục con).
    Returns: list of (display_path, absolute_path, size_bytes)
    """
    files_found = []
    
    # Tìm base path chung để hiển thị relative path
    base_path = Path(r"C:\Code\Keyboard_Blackpill\Dactyl_Left")

    for scan_dir in SCAN_DIRS:
        scan_path = Path(scan_dir)
        
        if not scan_path.exists():
            print(f"⚠️ Không tìm thấy: {scan_dir}")
            continue
            
        if not scan_path.is_dir():
            continue

        # Quét toàn bộ thư mục và thư mục con
        for root, dirs, files in os.walk(scan_path):
            # Bỏ qua thư mục ẩn và một số thư mục không cần thiết
            dirs[:] = [d for d in dirs if not d.startswith('.') and d.lower() not in ['build', 'debug', 'release']]

            for filename in sorted(files):
                file_path = Path(root) / filename
                ext = file_path.suffix.lower()

                # Chỉ lấy .c và .h
                if ext not in EXTENSIONS:
                    continue

                try:
                    size = file_path.stat().st_size
                except OSError:
                    continue

                # Bỏ qua file quá lớn
                if size > MAX_FILE_SIZE_KB * 1024:
                    print(f"⚠️ File quá lớn, bỏ qua: {file_path.name} ({size/1024:.1f}KB)")
                    continue

                # Bỏ qua file binary
                if is_binary(file_path):
                    continue

                # Tạo display path
                try:
                    display_path = file_path.relative_to(base_path)
                except ValueError:
                    display_path = file_path.name
                    
                files_found.append((str(display_path), file_path, size))

    # Sắp xếp theo thư mục rồi tên file
    files_found.sort(key=lambda x: x[0])
    
    return files_found


def read_file_content(file_path: Path) -> str:
    """Đọc nội dung file."""
    encodings = ['utf-8', 'latin-1', 'cp1252']
    for enc in encodings:
        try:
            with open(file_path, 'r', encoding=enc) as f:
                return f.read()
        except UnicodeDecodeError:
            continue
    return "/* Không thể đọc file */"


def format_size(size_bytes: int) -> str:
    """Format kích thước file."""
    if size_bytes < 1024:
        return f"{size_bytes}B"
    return f"{size_bytes / 1024:.1f}KB"


# ============================================================
# TẠO PROMPT
# ============================================================

def generate_prompt(question: str = "") -> str:
    """Tạo prompt hoàn chỉnh."""
    
    files = scan_directories()

    if not files:
        return "❌ Không tìm thấy file .c/.h nào trong các thư mục đã cấu hình"

    # === Xây dựng prompt ===
    lines = []
    
    # Header
    lines.append("=" * 60)
    lines.append(PROJECT_DESCRIPTION.upper())
    lines.append("=" * 60)
    lines.append("")
    
    # Thông tin
    total_size = sum(size for _, _, size in files)
    lines.append(f"📄 Số file: {len(files)}")
    lines.append(f"📦 Tổng kích thước: {format_size(total_size)}")
    lines.append("")
    
    # Danh sách file theo thư mục
    lines.append("## Cấu trúc")
    lines.append("```")
    
    current_dir = ""
    for display_path, _, size in files:
        parts = Path(display_path).parts
        if len(parts) > 1:
            dir_part = str(Path(*parts[:-1]))
            if dir_part != current_dir:
                current_dir = dir_part
                lines.append(f"\n[{current_dir}]")
        else:
            if current_dir != "[root]":
                current_dir = "[root]"
                lines.append(f"\n[root]")
        
        lines.append(f"  {Path(display_path).name} ({format_size(size)})")
    
    lines.append("```")
    lines.append("")
    
    # Nội dung từng file
    lines.append("## Source Code")
    lines.append("")
    
    total_lines = 0
    for display_path, abs_path, _ in files:
        content = read_file_content(abs_path)
        
        # Loại bỏ comment
        content = remove_comments(content)
        
        line_count = content.count('\n') + 1 if content else 0
        total_lines += line_count
        
        lines.append(f"### `{display_path}`")
        lines.append("```c")
        lines.append(content)
        lines.append("```")
        lines.append("")

    # Footer
    lines.append("-" * 40)
    lines.append(f"📊 Tổng: {len(files)} file, ~{total_lines} dòng (đã loại bỏ comment)")
    lines.append("-" * 40)
    
    # Câu hỏi
    if question:
        lines.append("")
        lines.append("## Yêu cầu")
        lines.append(question)
    
    lines.append("")
    
    return "\n".join(lines)


# ============================================================
# CLIPBOARD
# ============================================================

def copy_to_clipboard(text: str) -> bool:
    """Copy text vào clipboard (Windows)."""
    try:
        import subprocess
        process = subprocess.Popen(['clip'], stdin=subprocess.PIPE, shell=True)
        process.communicate(text.encode('utf-16le'))
        return True
    except Exception as e:
        print(f"Debug: {e}")
        return False


# ============================================================
# MAIN
# ============================================================

def main():
    print("=" * 50)
    print("  🎹 DACTYL KEYBOARD - PROMPT GENERATOR")
    print("=" * 50)
    print()
    
    # Kiểm tra thư mục
    print("📂 Thư mục quét (bao gồm thư mục con):")
    for d in SCAN_DIRS:
        exists = "✅" if Path(d).exists() else "❌"
        # Hiển thị tên ngắn gọn
        short_name = Path(d).relative_to(r"C:\Code\Keyboard_Blackpill\Dactyl_Left")
        print(f"   {exists} {short_name}")
    print()
    
    print("⏳ Đang quét file...")
    
    # Hỏi câu hỏi (tùy chọn)
    question = input("❓ Câu hỏi cho AI (Enter để bỏ qua): ").strip()
    
    # Tạo prompt
    print("\n⏳ Đang tạo prompt và loại bỏ comment...")
    prompt = generate_prompt(question)
    
    # Lưu file
    output_file = Path(r"C:\Code\Keyboard_Blackpill\Dactyl_Left\ai_prompt.txt")
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(prompt)
    
    size_kb = len(prompt.encode('utf-8')) / 1024
    print()
    print(f"✅ Đã lưu: {output_file}")
    print(f"📊 Kích thước: {size_kb:.1f} KB")
    
    # Copy clipboard
    if copy_to_clipboard(prompt):
        print("📋 Đã copy vào clipboard - Paste thẳng vào chat AI!")
    else:
        print("⚠️ Không thể copy clipboard")
    
    # Hỏi có muốn xem không
    print()
    show = input("👁️ Hiển thị prompt? (y/N): ").strip().lower()
    if show == 'y':
        print()
        print(prompt)


if __name__ == "__main__":
    main()