fips_begin_lib(roms)
    fipsutil_embed(breakout-roms.yml breakout-roms.h)
    fips_files(dummy.c)
fips_end_lib()