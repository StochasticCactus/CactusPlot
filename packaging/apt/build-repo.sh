#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds a signed apt repository from a folder of CactusPlot .deb packages.
#
#   packaging/apt/build-repo.sh DEB_DIR OUT_DIR [BASE_URL]
#
# The signing key must be in the gpg keyring; set APT_SIGNING_KEY to its id or fingerprint if the
# keyring holds more than one secret key. Requires apt-ftparchive (package apt-utils) and gpg.
#
# Layout of OUT_DIR, published under BASE_URL (the documentation workflow publishes it at
# https://<owner>.github.io/<repo>/apt):
#   cactusplot.gpg                    the public key (binary), for /usr/share/keyrings
#   cactusplot.sources                a ready-made /etc/apt/sources.list.d entry
#   pool/main/c/cactusplot/*.deb
#   dists/stable/{Release,InRelease,Release.gpg}
#   dists/stable/main/binary-<arch>/Packages{,.gz}
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 DEB_DIR OUT_DIR [BASE_URL]" >&2
    exit 2
fi
debs=$1
out=$2
base_url=${3:-https://stochasticcactus.github.io/CactusPlot/apt}
suite=stable
key_args=()
[ -n "${APT_SIGNING_KEY:-}" ] && key_args=(--local-user "$APT_SIGNING_KEY")

shopt -s nullglob
packages=("$debs"/*.deb)
if [ ${#packages[@]} -eq 0 ]; then
    echo "error: no .deb files in $debs" >&2
    exit 1
fi

rm -rf "$out"
mkdir -p "$out/pool/main/c/cactusplot"
cp "${packages[@]}" "$out/pool/main/c/cactusplot/"

cd "$out"
archs=$(for f in pool/main/c/cactusplot/*.deb; do dpkg-deb --field "$f" Architecture; done | sort -u | tr '\n' ' ')
archs=${archs% }
for arch in $archs; do
    dir="dists/$suite/main/binary-$arch"
    mkdir -p "$dir"
    apt-ftparchive --arch "$arch" packages pool > "$dir/Packages"
    gzip -9nk "$dir/Packages"
done

apt-ftparchive \
    -o "APT::FTPArchive::Release::Origin=CactusPlot" \
    -o "APT::FTPArchive::Release::Label=CactusPlot" \
    -o "APT::FTPArchive::Release::Suite=$suite" \
    -o "APT::FTPArchive::Release::Codename=$suite" \
    -o "APT::FTPArchive::Release::Components=main" \
    -o "APT::FTPArchive::Release::Architectures=$archs" \
    -o "APT::FTPArchive::Release::Description=CactusPlot packages" \
    release "dists/$suite" > "dists/$suite/Release.tmp"
mv "dists/$suite/Release.tmp" "dists/$suite/Release"

gpg --batch --yes "${key_args[@]}" --clearsign -o "dists/$suite/InRelease" "dists/$suite/Release"
gpg --batch --yes "${key_args[@]}" --armor --detach-sign -o "dists/$suite/Release.gpg" "dists/$suite/Release"
gpg --batch --yes --export ${APT_SIGNING_KEY:+"$APT_SIGNING_KEY"} > cactusplot.gpg
if [ ! -s cactusplot.gpg ]; then
    echo "error: could not export the public key" >&2
    exit 1
fi

cat > cactusplot.sources <<EOF
Types: deb
URIs: $base_url
Suites: $suite
Components: main
Signed-By: /usr/share/keyrings/cactusplot.gpg
EOF

echo "apt repository in $out: ${#packages[@]} package(s), architectures: $archs"
