#!/usr/bin/env bash
# setup-toolchain.sh — Instala el cross-compiler i686-elf-gcc en Fedora
# Solo necesitas ejecutarlo una vez. Tarda ~15-20 minutos.
set -e

PREFIX="$HOME/opt/cross"
TARGET="i686-elf"
BINUTILS_VER="2.41"
GCC_VER="13.2.0"

JOBS=$(nproc)
BUILD_DIR="$HOME/.fernandoos-build"

# ── Colores ──────────────────────────────────────────────────
RED='\033[0;31m'; GRN='\033[0;32m'; YEL='\033[1;33m'
CYN='\033[0;36m'; RST='\033[0m'

info()  { echo -e "${CYN}[INFO]${RST}  $*"; }
ok()    { echo -e "${GRN}[ OK ]${RST}  $*"; }
warn()  { echo -e "${YEL}[WARN]${RST}  $*"; }
error() { echo -e "${RED}[ERR ]${RST}  $*"; exit 1; }

echo ""
echo -e "${CYN}╔══════════════════════════════════════════════╗${RST}"
echo -e "${CYN}║   FernandoOS — Setup del Cross-Compiler      ║${RST}"
echo -e "${CYN}╚══════════════════════════════════════════════╝${RST}"
echo ""
info "Prefijo de instalación : $PREFIX"
info "Target                 : $TARGET"
info "Binutils               : $BINUTILS_VER"
info "GCC                    : $GCC_VER"
info "Núcleos de compilación : $JOBS"
echo ""

# ── 1. Dependencias ──────────────────────────────────────────
info "Instalando dependencias con dnf..."
sudo dnf install -y \
    nasm xorriso \
    gmp-devel mpc-devel mpfr-devel \
    texinfo isl-devel \
    2>&1 | grep -E "^(Instalando|Installing|Already|Ya)" || true
ok "Dependencias instaladas"

# ── 2. Directorio de trabajo ─────────────────────────────────
mkdir -p "$BUILD_DIR" "$PREFIX"
cd "$BUILD_DIR"

# ── 3. Binutils ──────────────────────────────────────────────
BINUTILS_TAR="binutils-${BINUTILS_VER}.tar.gz"
if [ ! -f "$BINUTILS_TAR" ]; then
    info "Descargando Binutils $BINUTILS_VER..."
    curl -# -O "https://ftp.gnu.org/gnu/binutils/$BINUTILS_TAR" \
      || wget -q --show-progress "https://ftp.gnu.org/gnu/binutils/$BINUTILS_TAR"
fi

if [ ! -d "binutils-${BINUTILS_VER}" ]; then
    info "Extrayendo Binutils..."
    tar xf "$BINUTILS_TAR"
fi

info "Compilando Binutils (puede tardar ~3 minutos)..."
mkdir -p build-binutils && cd build-binutils
../binutils-${BINUTILS_VER}/configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror \
    > /dev/null 2>&1
make -j"$JOBS" > /dev/null 2>&1
make install    > /dev/null 2>&1
cd ..
ok "Binutils $BINUTILS_VER instalado"

# ── 4. GCC ───────────────────────────────────────────────────
GCC_TAR="gcc-${GCC_VER}.tar.gz"
if [ ! -f "$GCC_TAR" ]; then
    info "Descargando GCC $GCC_VER (~85 MB, puede tardar)..."
    curl -# -O "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/$GCC_TAR" \
      || wget -q --show-progress "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/$GCC_TAR"
fi

if [ ! -d "gcc-${GCC_VER}" ]; then
    info "Extrayendo GCC..."
    tar xf "$GCC_TAR"
fi

info "Compilando GCC (puede tardar ~15 minutos)..."
export PATH="$PREFIX/bin:$PATH"
mkdir -p build-gcc && cd build-gcc
../gcc-${GCC_VER}/configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers \
    > /dev/null 2>&1
make -j"$JOBS" all-gcc          > /dev/null 2>&1
make -j"$JOBS" all-target-libgcc > /dev/null 2>&1
make install-gcc                 > /dev/null 2>&1
make install-target-libgcc       > /dev/null 2>&1
cd ..
ok "GCC $GCC_VER instalado"

# ── 5. Agregar al PATH ───────────────────────────────────────
SHELL_RC="$HOME/.bashrc"
[ -n "$ZSH_VERSION" ] && SHELL_RC="$HOME/.zshrc"

EXPORT_LINE="export PATH=\"\$HOME/opt/cross/bin:\$PATH\""
if ! grep -qF 'opt/cross/bin' "$SHELL_RC" 2>/dev/null; then
    echo ""                  >> "$SHELL_RC"
    echo "# FernandoOS cross-compiler" >> "$SHELL_RC"
    echo "$EXPORT_LINE"      >> "$SHELL_RC"
    info "PATH agregado a $SHELL_RC"
fi

export PATH="$PREFIX/bin:$PATH"

# ── 6. Verificación ──────────────────────────────────────────
echo ""
info "Verificando instalación..."
if "$PREFIX/bin/$TARGET-gcc" --version > /dev/null 2>&1; then
    ok "$TARGET-gcc: $($PREFIX/bin/$TARGET-gcc --version | head -1)"
else
    error "No se pudo verificar $TARGET-gcc"
fi

echo ""
echo -e "${GRN}╔══════════════════════════════════════════════╗${RST}"
echo -e "${GRN}║   ¡Toolchain instalado correctamente!        ║${RST}"
echo -e "${GRN}╚══════════════════════════════════════════════╝${RST}"
echo ""
echo "  Ahora ejecuta:"
echo ""
echo -e "    ${YEL}export PATH=\"\$HOME/opt/cross/bin:\$PATH\"${RST}"
echo -e "    ${YEL}cd ~/FernandoOS && make iso && make run-gui${RST}"
echo ""
