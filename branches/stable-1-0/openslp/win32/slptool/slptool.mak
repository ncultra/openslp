# Microsoft Developer Studio Generated NMAKE File, Based on slptool.dsp
!IF "$(CFG)" == ""
CFG=slptool - Win32 Debug
!MESSAGE No configuration specified. Defaulting to slptool - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "slptool - Win32 Release" && "$(CFG)" != "slptool - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "slptool.mak" CFG="slptool - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "slptool - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "slptool - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "slptool - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\slptool.exe"


CLEAN :
	-@erase "$(INTDIR)\slptool.idb"
	-@erase "$(INTDIR)\slptool.obj"
	-@erase "$(INTDIR)\slptool.pdb"
	-@erase "$(OUTDIR)\slptool.exe"
	-@erase "$(OUTDIR)\slptool.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /Zi /O2 /I "../../libslp" /I "../../common" /D "NDEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /D SLP_VERSION=\"1.0.9a\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\slptool.pdb" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\slptool.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib slp.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\slptool.pdb" /map:"$(INTDIR)\slptool.map" /debug /machine:I386 /out:"$(OUTDIR)\slptool.exe" /libpath:"../libslp/release" 
LINK32_OBJS= \
	"$(INTDIR)\slptool.obj"

"$(OUTDIR)\slptool.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "slptool - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\slptool.exe"


CLEAN :
	-@erase "$(INTDIR)\slptool.idb"
	-@erase "$(INTDIR)\slptool.obj"
	-@erase "$(INTDIR)\slptool.pdb"
	-@erase "$(OUTDIR)\slptool.exe"
	-@erase "$(OUTDIR)\slptool.ilk"
	-@erase "$(OUTDIR)\slptool.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /ZI /Od /I "../../libslp" /I "../../common" /D "_DEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /D SLP_VERSION=\"1.0.9a\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\slptool.pdb" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\slptool.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib slp.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\slptool.pdb" /map:"$(INTDIR)\slptool.map" /debug /machine:I386 /out:"$(OUTDIR)\slptool.exe" /libpath:"../libslp/debug" 
LINK32_OBJS= \
	"$(INTDIR)\slptool.obj"

"$(OUTDIR)\slptool.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("slptool.dep")
!INCLUDE "slptool.dep"
!ELSE 
!MESSAGE Warning: cannot find "slptool.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "slptool - Win32 Release" || "$(CFG)" == "slptool - Win32 Debug"
SOURCE=..\..\slptool\slptool.c

"$(INTDIR)\slptool.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 
