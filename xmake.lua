set_xmakever("3.0.0")
set_project("UltrawideCreationsMenu")
set_version("0.5.14")
set_arch("x64")
set_languages("c++23")
add_rules("mode.release")
includes("external/commonlibsf")

target("UltrawideCreationsMenu")
    add_rules("commonlibsf.plugin", {
        name = "Ultrawide Creations Menu",
        author = "JerKnuts",
        description = "Ultrawide storefront layout for Native Ultrawide Menus"
    })
    add_files("src/CreationsMenu.cpp")

