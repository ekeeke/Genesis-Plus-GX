@ECHO OFF
@FOR /F "delims=/" %%D in ('dir /b *.pcm') do raw2c "%%~nD".pcm
@FOR /F "delims=/" %%D in ('dir /b *.ogg') do raw2c "%%~nD".ogg
@FOR /F "delims=/" %%D in ('dir /b *.mp3') do raw2c "%%~nD".mp3