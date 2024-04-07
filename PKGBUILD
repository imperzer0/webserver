#!/bin/bash
# Maintained by imper <imperator999mcpe@gmail.com>

pkgname=webserver
pkgver=2.10
pkgrel=0
pkgdesc='Lightweight c++ web server for archlinux with ftp'
author="imperzer0"
arch=('any')
url=https://github.com/$author/$pkgname
license=('GPL3')
depends=('openssl' 'gcc' 'curl')
makedepends=('cmake' 'git' 'gcc' 'make' 'openssl' 'curl')

mongoose_pkgver="7.12"

_srcprefix="file://$(pwd)"
_libfiles=(
  "main.cpp"
  "server.cpp"
  "server.h"
  "constants.hpp"
  "tools.h"
  "tools.cpp"
  "config.cpp"
  "config.h"
)

_rcfiles=(
  "error.html"
  "index.html"
  "article.html"
  "register.html"
  "confirm.html"
  "dashboard.html"
  "bootstrap.css"
)

_ftpfiles=(
  "Findasio.cmake.patch"
  "PKGBUILD.fineftp"
  "custom_event_handler.hpp"
  "ftp_event_handler.h"
  "ftp_user.h"
  "ftp_session.cpp.patch"
)

source=(${source[@]} "$_srcprefix/CMakeLists.txt")
sha512sums=(${sha512sums[@]} "SKIP")

# shellcheck disable=SC2068
for _libfile in ${_libfiles[@]}; do
  source=(${source[@]} "$_srcprefix/sources/$_libfile")
  sha512sums=(${sha512sums[@]} "SKIP")
done

# shellcheck disable=SC2068
for _rcfile in ${_rcfiles[@]}; do
  source=(${source[@]} "$_srcprefix/resources/$_rcfile")
  sha512sums=(${sha512sums[@]} "SKIP")
done

# shellcheck disable=SC2068
for _ftpfile in ${_ftpfiles[@]}; do
  source=(${source[@]} "$_srcprefix/ftp/$_ftpfile")
  sha512sums=(${sha512sums[@]} "SKIP")
done

source=(${source[@]} "$_srcprefix/resources/favicon.ico")
_rcfiles=(${_rcfiles[@]} "favicon.ico")
sha512sums=(${sha512sums[@]} "72850225ffda45dff7f87645e80512ddce302fec175df7adb0e6d8d91f5f5a2131250fe91510b35f19cf05c1d229aa9eb8f71594c918555acb0418f3c2975cff")

source=(${source[@]} "$_srcprefix/resources/CascadiaMono.woff")
_rcfiles=(${_rcfiles[@]} "CascadiaMono.woff")
sha512sums=(${sha512sums[@]} "180d3248b16d5d3ed3aca598eb67e7edb8ec8553c21edafe96d9d268989c0d7896a615c7e67527d1fca913308e1b24a363a59c7826b7e341012e853736db4fa6")


external=(${external[@]} "https://raw.githubusercontent.com/cesanta/mongoose/$mongoose_pkgver/mongoose.c")
sha512sums=(${sha512sums[@]} "55011c1328abcfa91897b4b29aa5e1ab6c5f5f351442b8fc247ee9d9751dbbce1ab1892f9f996f140a18719a49412f45aad6c6bdbf7eed3348cb01982293daea")

external=(${external[@]} "https://raw.githubusercontent.com/cesanta/mongoose/$mongoose_pkgver/mongoose.h")
sha512sums=(${sha512sums[@]} "d13e87f326c285ce3aa7e837edb826d38f8bb73c72ac572af44f42447e9456f820f6d42e2248a33c18a3813b51f9e76ecfc99bba077d99a8f61d3b0b99b5cecd")

external=(${external[@]} "https://raw.githubusercontent.com/imperzer0/strscan/2f263154679e67ed44aa7fc4ae65829547e8290b/strscan.c")
sha512sums=(${sha512sums[@]} "0f62f4fb922325e53713d7013b709d65d50a2c94a440258b666d357bf35af2f5727b2014fbf955e15f169acc8e444eca84b309419886bbc98d0b563c4b795bc6")

external=(${external[@]} "https://raw.githubusercontent.com/imperzer0/strscan/2f263154679e67ed44aa7fc4ae65829547e8290b/strscan.h")
sha512sums=(${sha512sums[@]} "eb472a2ad6997f107d5f7db6f311294f34066f08fd6d7bf8c6be7f11322d8f26f50c678c6445c1f7643b32b3a28cfcd5576acc9b1af25ef33f41e3764165090d")

source=(${source[@]} ${external[@]})

_package_version=" ("$pkgver"-"$pkgrel")"

prepare() {
  mkdir sources
  # shellcheck disable=SC2068
  for _libfile in ${_libfiles[@]}; do
    mv "$_libfile" "sources/$_libfile"
  done

  mkdir resources
  # shellcheck disable=SC2068
  for _rcfile in ${_rcfiles[@]}; do
    mv "$_rcfile" "resources/$_rcfile"
  done

  mkdir ftp
  # shellcheck disable=SC2068
  for _ftpfile in ${_ftpfiles[@]}; do
    mv "$_ftpfile" "ftp/$_ftpfile"
  done

  cd ftp
  makepkg -sif --noconfirm -p PKGBUILD.fineftp
}

build() {
  cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DPACKAGE_VERSION="$_package_version" -DAPPNAME="$pkgname" .
  make -j 8
}

package() {
  install -Dm755 $pkgname "$pkgdir/usr/bin/$pkgname"
}
