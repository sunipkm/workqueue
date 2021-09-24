@set OUT_DIR=output
@set OUT_EXE=test
@set INCLUDES= /I .\
@set SOURCES_EXE=workqueue_nt.cpp ^
test.cpp
@set LIBS=
mkdir %OUT_DIR%
cl /nologo /Zi /EHsc /O2 /MD %INCLUDES% /D UNICODE /D _UNICODE %SOURCES_EXE% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%