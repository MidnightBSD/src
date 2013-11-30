
@echo off
echo Generating x86 for NASM assember

echo Bignum
cd crypto\bn\asm
perl x86.pl win32n > bn_win32.asm
cd ..\..\..

echo DES
cd crypto\des\asm
perl des-586.pl win32n > d_win32.asm
cd ..\..\..

echo "crypt(3)"

cd crypto\des\asm
perl crypt586.pl win32n > y_win32.asm
cd ..\..\..

echo Blowfish

cd crypto\bf\asm
perl bf-586.pl win32n > b_win32.asm
cd ..\..\..

echo CAST5
cd crypto\cast\asm
perl cast-586.pl win32n > c_win32.asm
cd ..\..\..

echo RC4
cd crypto\rc4\asm
perl rc4-586.pl win32n > r4_win32.asm
cd ..\..\..

echo MD5
cd crypto\md5\asm
perl md5-586.pl win32n > m5_win32.asm
cd ..\..\..

echo SHA1
cd crypto\sha\asm
perl sha1-586.pl win32n > s1_win32.asm
cd ..\..\..

echo RIPEMD160
cd crypto\ripemd\asm
perl rmd-586.pl win32n > rm_win32.asm
cd ..\..\..

echo RC5\32
cd crypto\rc5\asm
perl rc5-586.pl win32n > r5_win32.asm
cd ..\..\..

echo on

perl util\mkfiles.pl >MINFO
perl util\mk1mf.pl nasm VC-WIN32 >ms\nt.mak
perl util\mk1mf.pl dll nasm VC-WIN32 >ms\ntdll.mak
perl util\mk1mf.pl nasm BC-NT >ms\bcb.mak

perl util\mkdef.pl 32 libeay > ms\libeay32.def
perl util\mkdef.pl 32 ssleay > ms\ssleay32.def
