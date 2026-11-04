# SPDX-License-Identifier: GPL-2.0-or-later
# Installation of desktop integration files and CPack package generators.
include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/org.cactusplot.CactusPlot.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/org.cactusplot.CactusPlot.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES packaging/linux/cactusplot-mime.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages RENAME org.cactusplot.CactusPlot.xml)
    foreach(size 16 24 32 48 64 128 256 512)
        install(FILES packaging/icons/cactusplot-${size}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps RENAME org.cactusplot.CactusPlot.png)
    endforeach()
    install(FILES resources/cactusplot.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME org.cactusplot.CactusPlot.svg)
    install(FILES docs/cactusplot.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 OPTIONAL)
    # "cactusbat" behaves like gracebat (batch mode, no GUI)
    install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink cactusplot \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/cactusbat\")")
endif()

install(FILES LICENSE README.md THIRD_PARTY.md DESTINATION ${CMAKE_INSTALL_DOCDIR} OPTIONAL)

set(CPACK_PACKAGE_NAME "cactusplot")
set(CPACK_PACKAGE_VENDOR "CactusPlot contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Beautiful 2D scientific plotting, compatible with Grace/xmgrace")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "CactusPlot contributors <noreply@cactusplot.org>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CactusPlot")
set(CPACK_PACKAGE_EXECUTABLES "cactusplot;CactusPlot")
set(CPACK_STRIP_FILES ON)
set(CPACK_SOURCE_IGNORE_FILES "/build.*/;/\\\\.git/;\\\\.user$")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "CactusPlot")
    set(CPACK_NSIS_PACKAGE_NAME "CactusPlot ${PROJECT_VERSION}")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/packaging/windows/cactusplot.ico")
    set(CPACK_NSIS_MUI_UNIIcon "${CMAKE_SOURCE_DIR}/packaging/windows/cactusplot.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\cactusplot.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\CactusPlot.lnk' '$INSTDIR\\\\bin\\\\cactusplot.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\CactusPlot.lnk'")
    # file associations
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.cplot' '' 'CactusPlot.Project'
        WriteRegStr HKCR 'CactusPlot.Project' '' 'CactusPlot project'
        WriteRegStr HKCR 'CactusPlot.Project\\\\DefaultIcon' '' '$INSTDIR\\\\bin\\\\cactusplot.exe,0'
        WriteRegStr HKCR 'CactusPlot.Project\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\cactusplot.exe\\\" \\\"%1\\\"'
        WriteRegStr HKCR '.agr\\\\OpenWithProgids' 'CactusPlot.Project' ''
        WriteRegStr HKCR '.xvg\\\\OpenWithProgids' 'CactusPlot.Project' ''
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR 'CactusPlot.Project'
        DeleteRegKey HKCR '.cplot'
    ")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "CactusPlot")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/packaging/macos/CactusPlot.icns")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_SECTION "science")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland, qt6-image-formats-plugins")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    if(CACTUS_BUILD_NUMBER)
        # 0.9.0-12 sorts after 0.9.0-11 and after a plain 0.9.0, so apt/dnf replace older builds
        set(CPACK_DEBIAN_PACKAGE_RELEASE "${CACTUS_BUILD_NUMBER}")
        set(CPACK_RPM_PACKAGE_RELEASE "${CACTUS_BUILD_NUMBER}")
    endif()
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/share/applications /usr/share/icons /usr/share/icons/hicolor /usr/share/metainfo /usr/share/mime
        /usr/share/mime/packages /usr/share/man /usr/share/man/man1)
endif()

include(CPack)
