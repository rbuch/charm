CMK_CPP_CHARM='/lib/cpp -P'
CMK_CPP_C='gcc -E'
CMK_CC='gcc -D_REENTRANT'
CMK_CXX='g++ -D_REENTRANT'
CMK_CXXPP="$CMK_CXX -x c++ -E "
CMK_CF77='f77'
CMK_CF90='f90'
CMK_RANLIB='ranlib'
CMK_LIBS='-lckqt -lmpi '
CMK_LD_LIBRARY_PATH="-rpath $CHARMLIBSO/"
CMK_QT='axp-gcc'
CMK_XIOPTS='-ansi'
CMK_F90LIBS="-lUfor -lfor -lFutil"
CMK_F77LIBS="$CMK_F90LIBS "

