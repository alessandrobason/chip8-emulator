add_definitions(-D${sokol_backend})

fips_begin_app(chip8 windowed)
    fips_files(chip8_font.c chip8.c main.c)
    fips_deps(graphics roms)
fips_end_app()