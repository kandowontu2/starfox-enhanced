#pragma once
#include <windows.h>
#include <d3d12.h>
#include <filesystem>

// Analytically rendered moving foreground/background fixture. Not a gameplay
// integration: this verifies real SDK evaluation, GPU completion and output.
void evaluate_dlss(HMODULE module, ID3D12Device* device,
                   const std::filesystem::path& output_directory);
