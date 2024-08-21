
# EFI Kernel Extractor

This project provides a C utility to extract and decompress a Linux kernel image from an EFI application file, specifically designed to handle ARM64 kernels that are embedded within EFI zboot images. The utility verifies the image, decompresses it if necessary, and saves the decompressed kernel to an output file.

## Features

- **EFI zboot Image Handling**: Detects and processes Linux EFI zboot images.
- **Decompression**: Supports gzip compression format for the kernel image.
- **ARM64 Verification**: Ensures that the extracted image is a valid ARM64 kernel before saving.

## Getting Started

### Prerequisites

- **Compiler**: A C compiler like `gcc`.
- **Build System**: [Meson](https://mesonbuild.com/) and [Ninja](https://ninja-build.org/).
- **Libraries**: This utility relies on the following libraries:
  - `glib-2.0`
  - `zlib`

#### Installing Dependencies on Fedora

You can install the necessary dependencies on Fedora using:
```bash
sudo dnf install gcc meson ninja-build glib2-devel zlib-devel
```

#### Installing Dependencies on Ubuntu

You can install the necessary dependencies on Ubuntu using:
```bash
sudo apt-get install build-essential meson ninja-build libglib2.0-dev zlib1g-dev
```

### Building the Utility

To build the program, follow these steps:

1. Clone the repository and navigate to the project directory:
    ```bash
    git clone https://github.com/yourusername/efi-kernel-extractor.git
    cd efi-kernel-extractor
    ```

2. Configure the build directory using Meson:
    ```bash
    meson setup build
    ```

3. Build the project using Ninja:
    ```bash
    meson compile -C build
    ```

The compiled binary will be available in the `build` directory.

### Usage

Once compiled, the program can be run from the command line with the following syntax:
```bash
./build/efi-kernel-extractor <input_file> <output_file>
```

- **`input_file`**: The path to the EFI application containing the compressed kernel image.
- **`output_file`**: The path to the output file where the decompressed kernel will be saved.

### Example

```bash
./build/efi-kernel-extractor efi_image.efi vmlinuz
```

This will extract the kernel image from `efi_image.efi` and save it as `vmlinuz` if it is a valid ARM64 compressed image.

## Error Handling

The utility includes error checks for:
- Incorrect or unsupported EFI zboot images.
- Issues during decompression.
- Verification failures for the ARM64 kernel image.

If an error occurs, a descriptive message will be printed to `stderr`, and the program will exit with a non-zero status.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## Acknowledgments

- **Unpacking functionality** is derived from `qemu`, originally authored by Fabrice Bellard.
- **Gunzip functionality** is derived from `u-boot` by Wolfgang Denk and Semihalf.
- **EFI zboot image format** based on the Linux upstream specification.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub if you have suggestions or improvements.

## Contact

For any questions or issues, feel free to open an issue on this repository or contact the maintainer.
