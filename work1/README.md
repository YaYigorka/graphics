# Лабораторная работа №1 — правильный додекаэдр

Вариант 10.

## Требования

- macOS
- Xcode Command Line Tools
- Vulkan SDK от LunarG (включая MoltenVK и `glslc`)
- Homebrew
- CMake
- GLFW

## Установка зависимостей

```bash
xcode-select --install
brew install cmake glfw
```

```bash
source "$HOME/VulkanSDK/<версия>/setup-env.sh"
```

Проверить установку:

```bash
vulkaninfo
```

## Сборка и запуск

Из каталога проекта:

```bash
cmake -S . -B build
cmake --build build
./build/dodecahedron
```

## Управление

Фигура непрерывно вращается. Стрелки влево и вправо меняют скорость/направление вращения.
