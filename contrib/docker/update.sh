#!/bin/bash
BASE_IMAGE=debian:stretch
EBUSD_VERSION=`cat ../../VERSION`

archs='amd64 i386 arm32v7:arm arm64v8:aarch64'

function replaceTemplate () {
  prefix=
  suffix=
  qemu_from_line=
  qemu_from_copy=
  extra_rm=
  if [ "$arch" != "amd64" ]; then
    qemu="${arch##*:}"
    arch="${arch%%:*}"
    prefix="$arch/"
    suffix=".$arch"
    qemu_from_line="FROM multiarch/qemu-user-static as qemu"
    qemu_from_copy="COPY --from=qemu /usr/bin/qemu-$qemu-static /usr/bin/"
    extra_rm=" /usr/bin/qemu-$qemu-static"
  fi
  sed \
    -e "s#%QEMU_FROM_LINE%#${qemu_from_line}#g" \
    -e "s#%BASE_IMAGE%#${prefix}${BASE_IMAGE}#g" \
    -e "s#%QEMU_FROM_COPY%#${qemu_from_copy}#g" \
    -e "s#%EBUSD_MAKE%#${make}#g" \
    -e "s#%EBUSD_VERSION%#${EBUSD_VERSION}#g" \
    -e "s#%EBUSD_VERSION_VARIANT%#${version_variant}#g" \
    -e "s#%EBUSD_ARCH%#${arch}#g" \
    -e "s#%EXTRA_RM%#${extra_rm}#g" \
    -e "s#%EBUSD_UPLOAD_LINE%#${upload_line}#g" \
    Dockerfile.template > "${dir}Dockerfile${suffix}"
  echo "updated ${dir}Dockerfile${suffix}"
}

# devel updates
version_variant='-devel'
make='./make_debian.sh'
dir=''
upload_line=''
for arch in $archs; do
  replaceTemplate
done

# release updates
version_variant=''
make='./make_all.sh'
dir='release/'
upload_line='RUN if [ -n "\$UPLOAD_URL" ] \&\& [ -n "\$UPLOAD_CREDENTIALS" ]; then for img in ebusd-*.deb; do echo "upload \$img"; curl -u "\$UPLOAD_CREDENTIALS" -s -X POST --data-binary "@\$img" -H "Content-Type: application/octet-stream" "\$UPLOAD_URL/\$img?a=\$EBUSD_ARCH\&v=\$EBUSD_VERSION"; done; if [ -n "\$UPLOAD_ONLY" ]; then echo "stopping for upload only"; exit 139; fi fi'
for arch in $archs; do
  replaceTemplate
done

