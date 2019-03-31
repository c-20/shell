shell draws text to the display using the openvg graphics layer (...also EGL?)

drawing requires the ukai.ttf file to be copied to RAM

shell uses the freetype library to convert glyphs to paths: requires ft2build.h

users runs shell in a multi-user environment

frame calls users to call shell which creates an image for frame to return
