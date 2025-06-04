#!/bin/bash
pkgname="webserver"
pkgver="2.11"
makedepends=('build-essential' 'cmake' 'make' 'gcc' 'g++' 'openssl' 'libssl-dev' 'curl' 'libcurl4-openssl-dev')

pkgdir="$(pwd)/$pkgname"
srcdir="$(pwd)/src"


# Default Parameters
DEBUG_BUILD=0
INSTALL=0
JUSTLOAD=0

# Parse CL Arguments
__POSITIONAL_ARGS__=()

while [[ $# -gt 0 ]]; do
  case $1 in
    -d)
      DEBUG_BUILD=1
      shift # past argument
      ;;
    -i)
      INSTALL=1
      shift # past argument
      ;;
    -l)
      JUSTLOAD=1
      shift # past argument
      ;;
    -*)
      echo "Unknown option $1"
      echo ""
      echo "Available options:"
      echo "-d  : Build as Debug"
      echo "-i  : Install right away"
      echo "-l  : Just Load the functions without executing them"
      exit 1
      ;;
    *)
      __POSITIONAL_ARGS__+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

set -- "${__POSITIONAL_ARGS__[@]}" # restore positional parameters



MAKEPKG_install_deps() {
  echo "-> MAKEPKG_install_deps($pkgname)"

  sudo apt-get install --yes ${makedepends[*]} || exit 10;
  sudo apt-mark auto ${makedepends[*]};
}

MAKEPKG_init_dirs() {
  echo "-> MAKEPKG_init_dirs($pkgname) in $(pwd)"

  rm -rfv "$srcdir" && echo "Removed old source directory" || exit 1;
  rm -rfv "$pkgdir" && echo "Removed old package directory" || exit 1;
  mkdir -pv "$srcdir";
  mkdir -pv "$pkgdir";

  cd "$srcdir" || exit 1;
}

MAKEPKG_copy_files() {
  _srcprefix="../.."
  echo "-> MAKEPKG_copy_files($pkgname:$_srcprefix->$srcdir) in $(pwd)"

  cp -rfv "$_srcprefix/CMakeLists.txt" "$srcdir/.";

  cp -rfv "$_srcprefix/sources/" "$srcdir/.";
  cp -rfv "$_srcprefix/resources/" "$srcdir/.";
  cp -rfv "$_srcprefix/ftp/" "$srcdir/.";
  cp -rfv "$_srcprefix/mongoose/" "$srcdir/.";
  cp -rfv "$_srcprefix/strscan/" "$srcdir/.";



  source=()
  sha512sums=()

  source=(${source[*]} "resources/favicon.ico")
  sha512sums=(${sha512sums[*]} "72850225ffda45dff7f87645e80512ddce302fec175df7adb0e6d8d91f5f5a2131250fe91510b35f19cf05c1d229aa9eb8f71594c918555acb0418f3c2975cff")

  source=(${source[*]} "resources/CascadiaMono.woff")
  sha512sums=(${sha512sums[*]} "180d3248b16d5d3ed3aca598eb67e7edb8ec8553c21edafe96d9d268989c0d7896a615c7e67527d1fca913308e1b24a363a59c7826b7e341012e853736db4fa6")

  for i in ${!source[*]}; do
    echo -n "Checking sha512sum: ";
    echo "${sha512sums[i]} $srcdir/${source[i]}" | sha512sum --check || exit 2;
  done
}

MAKEPKG_prepare() {
  echo "-> MAKEPKG_prepare($pkgname) in $(pwd)"

  _cwd="$(pwd)"

  cd "ftp/debpackage" || exit 3;

  echo " >> Executing makepkg.sh in [$(pwd)]..."
  bash "makepkg.sh" && echo "Success" || exit 3;

  sudo dpkg --unpack "fineftp-server.deb" || exit 3;
  sudo apt-get install --yes --fix-broken || exit 3;

  sudo apt-mark auto fineftp-server

  cd "$_cwd" || exit 3;
}

MAKEPKG_build() {
  echo "-> MAKEPKG_build($pkgname\[v$pkgver]) in $(pwd)"

  _package_version=" ($pkgver)"
  _BUILD_TYPE=Release
  if [ $DEBUG_BUILD -eq 1 ]; then
    _BUILD_TYPE=Debug
  fi
  cmake -DCMAKE_BUILD_TYPE="$_BUILD_TYPE" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DPACKAGE_VERSION="$_package_version" -DAPPNAME="$pkgname" . || exit 4;
  make -j 8 || exit 5;
}

MAKEPKG_package() {
  echo "-> MAKEPKG_package($pkgname) in $(pwd)"

  rm -rfv "${pkgdir:?}/usr/bin/$pkgname" || exit 6;
  install -Dm755 "$srcdir/$pkgname" "${pkgdir:?}/usr/bin/$pkgname" || exit 6;
}

MAKEPKG_create_control_file() {
  mkdir -p "$pkgdir/DEBIAN"
  cat <<EOF > "$pkgdir/DEBIAN/control"
Package: $pkgname
Version: $pkgver
Essential: no
Priority: optional
Maintainer: imperzer0 <dmytroperets@gmail.com>
Architecture: $(dpkg --print-architecture)
Depends: gcc, g++, openssl, libssl-dev, curl, libcurl4-openssl-dev, fineftp-server
Description: Lightweight web server written in c++ for linux with ftp support
EOF
}

MAKEPKG_dpkg_build() {
  echo "-> MAKEPKG_dpkg_build($pkgname) in $(pwd)"

  cd ..
  dpkg-deb --verbose --build "./$pkgname" || exit 8;
}


if [ $JUSTLOAD -eq 0 ]; then
  MAKEPKG_install_deps;
  MAKEPKG_init_dirs;
  MAKEPKG_copy_files;
  MAKEPKG_prepare;
  MAKEPKG_build;
  MAKEPKG_package;
  MAKEPKG_create_control_file;
  MAKEPKG_dpkg_build;
fi


if [ $INSTALL -eq 1 ]; then
  echo "[Finishing up] Installing package {./$pkgname.deb}..."
  sudo dpkg -i "./$pkgname.deb" || exit 200;
fi