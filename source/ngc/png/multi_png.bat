@ECHO OFF
@FOR /F "delims=/" %%D in ('dir /b *.png') do raw2c "%%~nD".png
