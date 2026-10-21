#!/usr/bin/env bash
set -euo pipefail
rug_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
rug_out="$rug_root/build"
mkdir -p "$rug_out"
rug_modules=(Qt6Core Qt6Gui Qt6Qml Qt6Quick Qt6DBus)
if [[ -n "${QT_INCLUDE_DIR:-}" ]]; then
    rug_flags=(-I"$QT_INCLUDE_DIR")
    for rug_module in QtCore QtGui QtQml QtQuick QtQmlIntegration QtDBus; do
        rug_flags+=(-I"$QT_INCLUDE_DIR/$rug_module")
    done
    rug_libs=()
    for rug_module in Core Gui Qml Quick DBus; do
        rug_libs+=("${QT_LIB_DIR:-/usr/lib64}/libQt6$rug_module.so.6")
    done
else
    pkg-config --exists "${rug_modules[@]}" || { echo 'Install the Qt 6 development packages listed in README.md.' >&2; exit 1; }
    read -r -a rug_flags <<< "$(pkg-config --cflags "${rug_modules[@]}")"
    read -r -a rug_libs <<< "$(pkg-config --libs "${rug_modules[@]}")"
fi
rug_moc=${QT_MOC:-}
if [[ -z "$rug_moc" ]]; then
    for rug_candidate in /usr/lib64/qt6/libexec/moc /usr/lib/qt6/libexec/moc /usr/lib/qt6/bin/moc; do
        if [[ -x "$rug_candidate" ]]; then rug_moc=$rug_candidate; break; fi
    done
fi
[[ -x "$rug_moc" ]] || { echo 'Qt 6 moc not found; set QT_MOC.' >&2; exit 1; }
rug_qsb=${QT_QSB:-$(command -v qsb-qt6 || command -v qsb6 || command -v qsb || true)}
[[ -x "$rug_qsb" ]] || { echo 'Qt 6 qsb not found; install qt6-qtshadertools-devel or set QT_QSB.' >&2; exit 1; }
cp -a "$rug_root/plasma/." "$rug_out/package/"
rug_native="$rug_out/package/contents/ui/native-v5"
mkdir -p "$rug_native"
"$rug_moc" "${rug_flags[@]}" "$rug_root/src/desktoprug.cpp" -o "$rug_out/desktoprug.moc"
"${CXX:-c++}" -std=c++17 -O2 -DNDEBUG -fPIC -shared -I"$rug_out" "${rug_flags[@]}" "$rug_root/src/desktoprug.cpp" "${rug_libs[@]}" -o "$rug_native/libdesktopruglock.so"
for rug_stage in vert frag; do
    "$rug_qsb" --glsl '100 es,120,150' --hlsl 50 --msl 12 -o "$rug_native/cloth.$rug_stage.qsb" "$rug_root/src/shaders/cloth.$rug_stage"
done
printf '%s\n' 'module DesktopRugLock' 'plugin desktopruglock' > "$rug_native/qmldir"
echo "Built $rug_out/package"
if [[ "${1:-}" == --test ]]; then
    if [[ -n "${QT_INCLUDE_DIR:-}" ]]; then
        rug_test_flags=(-I"$QT_INCLUDE_DIR/QtTest")
        rug_test_libs=("${QT_LIB_DIR:-/usr/lib64}/libQt6Test.so.6")
    else
        read -r -a rug_test_flags <<< "$(pkg-config --cflags Qt6Test)"
        read -r -a rug_test_libs <<< "$(pkg-config --libs Qt6Test)"
    fi
    "${CXX:-c++}" -std=c++17 -O2 -fPIC -DQT_GUI_LIB "${rug_flags[@]}" "${rug_test_flags[@]}" "$rug_root/tests/locking.cpp" "${rug_libs[@]}" "${rug_test_libs[@]}" -o "$rug_out/test-locking"
    QT_QPA_PLATFORM=offscreen "$rug_out/test-locking" "$rug_root/tests/Preview.qml"
fi
