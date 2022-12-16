#!/bin/bash
# Maintained by imper <imperator999mcpe@gmail.com>

pkgname=webserver
pkgver=2.5
pkgrel=1
pkgdesc='Lightweight c++ web server for archlinux with ftp'
author="imperzer0"
arch=('any')
url=https://github.com/$author/$pkgname
license=('GPL3')
depends=('openssl' 'gcc' 'curl')
makedepends=('cmake' 'git' 'gcc' 'make' 'openssl' 'curl')

_srcprefix="local:/"
_libfiles=("CMakeLists.txt" "main.cpp"
	"server.cpp" "server.h" "constants.hpp"
	"config.cpp" "config.h"
)

_rcfiles=(
	"resources/error.html"
	"resources/favicon.ico"
	"resources/index.html"
	"resources/article.html"
	"resources/register.html"
	"resources/confirm.html"
)

_ftpfiles=(
	"Findasio.cmake.patch"
	"PKGBUILD.fineftp"
)

# shellcheck disable=SC2068
for _libfile in ${_libfiles[@]}; do
	source=(${source[@]} "$_srcprefix/$_libfile")
	sha512sums=(${sha512sums[@]} "SKIP")
done

# shellcheck disable=SC2068
for _rcfile in ${_rcfiles[@]}; do
	source=(${source[@]} "$_srcprefix/$_rcfile")
	sha512sums=(${sha512sums[@]} "SKIP")
done

# shellcheck disable=SC2068
for _ftpfile in ${_ftpfiles[@]}; do
	source=(${source[@]} "$_srcprefix/$_ftpfile")
	sha512sums=(${sha512sums[@]} "SKIP")
done

external=(${external[@]} "https://raw.githubusercontent.com/cesanta/mongoose/0a265e79a67d7bfcdca27f2ccb98ccb474677ec6/mongoose.c")
sha512sums=(${sha512sums[@]} "2fb2abd382aecc86ae4a9474fa8c40fe3f59e32cba20259d4588815f67d9658cbbe5f87a1b3eb18c5a8defe38becce5e38f1e08b35da2335a3e441e2346666e7")

external=(${external[@]} "https://raw.githubusercontent.com/cesanta/mongoose/0a265e79a67d7bfcdca27f2ccb98ccb474677ec6/mongoose.h")
sha512sums=(${sha512sums[@]} "931f723081512935f5bcb8737dd280408cfcb161d9ebff72657654bc6006c3b2d283eae1c9d2f1ce8db5318fa17d7434280827ef7e3935401825b5359917f9fb")

external=(${external[@]} "https://raw.githubusercontent.com/imperzer0/strscan/2f263154679e67ed44aa7fc4ae65829547e8290b/strscan.c")
sha512sums=(${sha512sums[@]} "0f62f4fb922325e53713d7013b709d65d50a2c94a440258b666d357bf35af2f5727b2014fbf955e15f169acc8e444eca84b309419886bbc98d0b563c4b795bc6")

external=(${external[@]} "https://raw.githubusercontent.com/imperzer0/strscan/2f263154679e67ed44aa7fc4ae65829547e8290b/strscan.h")
sha512sums=(${sha512sums[@]} "eb472a2ad6997f107d5f7db6f311294f34066f08fd6d7bf8c6be7f11322d8f26f50c678c6445c1f7643b32b3a28cfcd5576acc9b1af25ef33f41e3764165090d")

source=(${source[@]} ${external[@]})

_package_version=" ("$pkgver"-"$pkgrel")"

prepare() {
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
