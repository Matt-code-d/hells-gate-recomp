#!/usr/bin/env bash
set -euo pipefail

# Dependencias mínimas/útiles para hells-gate-recomp + ReXGlue en Ubuntu.
# Mantiene las librerías de desarrollo para evitar faltantes al compilar.
# Clang usado por el proyecto: 22.

if [[ ! -r /etc/os-release ]]; then
  echo "ERROR: No se pudo detectar la distribución."
  exit 1
fi

. /etc/os-release

CODENAME="${VERSION_CODENAME:-}"
if [[ -z "$CODENAME" ]]; then
  echo "ERROR: No se pudo detectar VERSION_CODENAME."
  exit 1
fi

echo "==> Ubuntu: ${PRETTY_NAME:-desconocido} (${CODENAME})"

sudo apt update


# Herramientas realmente usadas por el flujo.
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  git \
  python3 \
  wget \
  gnupg \
  ca-certificates

# Librerías de desarrollo: X11/Wayland, audio, Vulkan y GTK.
sudo apt install -y \
  libgtk-3-dev \
  libxss-dev \
  libx11-dev \
  libxext-dev \
  libxrandr-dev \
  libxcursor-dev \
  libxi-dev \
  libxinerama-dev \
  libxtst-dev \
  libx11-xcb-dev \
  libxcb1-dev \
  libxkbcommon-dev \
  libwayland-dev \
  wayland-protocols \
  libasound2-dev \
  libpulse-dev \
  libvulkan-dev

# ============================
# Instalar PowerShell 7
# ============================

sudo apt update
sudo apt install -y wget apt-transport-https software-properties-common

source /etc/os-release

wget -q \
  "https://packages.microsoft.com/config/ubuntu/$VERSION_ID/packages-microsoft-prod.deb" \
  -O packages-microsoft-prod.deb

sudo dpkg -i packages-microsoft-prod.deb
rm -f packages-microsoft-prod.deb




sudo apt update
sudo snap install powershell --classic

# ReXGlue requiere CMake >= 3.25. En Ubuntu 22.04 el de Ubuntu es demasiado viejo.
CMAKE_VERSION="$(cmake --version | awk 'NR==1 {print $3}')"
if dpkg --compare-versions "$CMAKE_VERSION" lt "3.25"; then
  echo "==> CMake ${CMAKE_VERSION} es menor que 3.25; instalando CMake de Kitware..."

  wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor \
    | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null

  echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ ${CODENAME} main" \
    | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null

  sudo apt update
  sudo apt install -y cmake
fi

# Clang 22 desde apt.llvm.org. llvm.sh detecta el codename de Ubuntu.
if ! command -v clang-22 >/dev/null 2>&1; then
  echo "==> Instalando Clang 22 desde apt.llvm.org..."
  TMP_LLVM="$(mktemp)"
  wget -q https://apt.llvm.org/llvm.sh -O "$TMP_LLVM"
  chmod +x "$TMP_LLVM"
  sudo "$TMP_LLVM" 22
  rm -f "$TMP_LLVM"
fi



# Conservamos lld y libc++/libc++abi, aunque nuestro build actual usa libstdc++.
sudo apt update
sudo apt install -y \
  clang-22 \
  lld-22 \
  libc++-22-dev \
  libc++abi-22-dev


sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 220
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 220

if [ -f /usr/bin/ld.lld-22 ]; then
  sudo update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/ld.lld-22 220
fi

# En Ubuntu 22.04, usar libstdc++ 13 para las partes C++23 de ReXGlue.
# Clang 22 sigue siendo el compilador.
if [[ "$CODENAME" == "jammy" ]]; then
  echo "==> Jammy detectado: instalando libstdc++ 13 para C++23..."
  sudo apt install -y software-properties-common
  sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
  sudo apt update
  sudo apt install -y g++-13 libstdc++-13-dev
fi

echo
echo "===== Toolchain ====="
clang --version | head -n 1
clang++ --version | head -n 1
cmake --version | head -n 1
ninja --version
pwsh --version
echo
echo "Dependencias listas."
echo "Usa /usr/bin/clang-22 y /usr/bin/clang++-22 al configurar CMake."
