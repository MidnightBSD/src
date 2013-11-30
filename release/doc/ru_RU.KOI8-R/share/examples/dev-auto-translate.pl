#!/usr/bin/perl
# $FreeBSD: src/release/doc/ru_RU.KOI8-R/share/examples/dev-auto-translate.pl,v 1.3 2005/06/30 12:07:29 den Exp $
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/share/examples/dev-auto-translate.pl,v 1.4 2005/06/30 12:11:18 den Exp $
#
# Auto-translate some device entities from English to Russian (KOI8-R)
#
# Example:
# cd /usr/src/release/doc/ru_RU.KOI8-R
# perl share/examples/dev-auto-translate.pl -o share/sgml/dev-auto-ru.sgml < ../share/sgml/dev-auto.sgml
#
# This script is maintained only in HEAD branch.

use Getopt::Std;
use POSIX qw(fprintf);

$OutputFile = "";
$isOutputFile = 0;

if (getopts('o:')) {
  chomp($OutputFile = $opt_o);
  $isOutputFile = 1;
}

if($isOutputFile) {
  open TRANSLATED, "< $OutputFile"  or die "Can't open $OutputFile: $!";

  # check for already translated entities
  undef %translated;
  while(<TRANSLATED>) {
    next if !/hwlist\.([0-9a-f]+)/;
    $translated{$1} = 1;
  }

  close(TRANSLATED);

  open OUTPUTFILE, ">> $OutputFile"  or die "Can't open $OutputFile: $!";
}

# translate some entities
while (<>) {
next if !/&man\..*\.[0-9];/;
s/Controllers supported by the (&man\..*\.[0-9];) driver include:/�����������, �������������� ��������� $1, �������:/;
s/The (&man\..*\.[0-9];) driver supports the following SCSI controllers:/������� $1 ������������ ��������� ����������� SCSI:/;
s/The (&man\..*\.[0-9];) driver supports SCSI controllers including:/������� $1 ������������ ����������� SCSI, �������:/;
s/The (&man\..*\.[0-9];) driver supports the following SCSI host adapters:/������� $1 ������������ ��������� ���� �������� SCSI:/;
s/The (&man\..*\.[0-9];) driver supports the following SCSI host adapter chips and SCSI controller cards:/������� $1 ������������ ��������� ���� �������� SCSI � ����� ������������ SCSI:/;
s/The (&man\..*\.[0-9];) driver supports the following:/������� $1 ������������ ���������:/;
s/The adapters currently supported by the (&man\..*\.[0-9];) driver include the following:/��������, �������������� � ��������� ����� ��������� $1, ������� ���������:/;
s/The following cards are among those supported by the (&man\..*\.[0-9];) driver:/��������� �������� ������ � ����� �������������� ��������� $1:/;
s/The following cards are among those supported by the (&man\..*\.[0-9];) module:/��������� �������� ������ � ����� �������������� ������� $1:/;
s/Adapters supported by the (&man\..*\.[0-9];) driver include:/��������, �������������� ��������� $1, �������:/;
s/Cards supported by the (&man\..*\.[0-9];) driver include:/�����, �������������� ��������� $1, �������:/;
s/The (&man\..*\.[0-9];) driver supports the following card models:/������� $1 ������������ ��������� ������ ����:/;
s/The (&man\..*\.[0-9];) driver provides support for the following chipsets:/������� $1 ������������ ��������� ������ ���������:/;
s/The following NICs are known to work with the (&man\..*\.[0-9];) driver at this time:/��������� ������� ����� �������� � ��������� $1:/; 
s/The (&man\..*\.[0-9];) driver provides support for the following RAID adapters:/������� $1 ������������ ��������� RAID ��������:/; 
s/The (&man\..*\.[0-9];) driver supports the following Ethernet NICs:/������� $1 ������������ ��������� ������� ����� Ethernet:/; 
s/Cards supported by (&man\..*\.[0-9];) driver include:/�����, �������������� $1, �������:/; 
s/The (&man\..*\.[0-9];) driver supports the following adapters:/������� $1 ������������ ��������� ��������:/; 
s/The (&man\..*\.[0-9];) driver supports the following ATA RAID controllers:/������� $1 ������������ ��������� ����������� ATA RAID:/; 
s/The following controllers are supported by the (&man\..*\.[0-9];) driver:/��������� $1 �������������� ��������� �����������:/;
s/The (&man\..*\.[0-9];) driver supports the following cards:/������� $1 ������������ ��������� �����:/;
s/The SCSI controller chips supported by the (&man\..*\.[0-9];) driver can be found onboard on many systems including:/���������� SCSI �����������, �������������� ��������� $1, ����� ���� �������� �� ������ �������, �������:/;
s/The following devices are currently supported by the (&man\..*\.[0-9];) driver:/��������� $1 � ��������� ����� �������������� ��������� ����������:/;
s/The (&man\..*\.[0-9];) driver supports cards containing any of the following chips:/������� $1 ������������ �����, ���������� ����� �� ��������� ���������:/;
s/The (&man\..*\.[0-9];) driver supports the following soundcards:/������� $1 ������������ ��������� �������� �����:/;
s/The (&man\..*\.[0-9];) driver supports audio devices based on the following chipset:/������� $1 ������������ �������� ����������, ���������� �� ��������� ������ ���������:/;
s/The (&man\..*\.[0-9];) driver supports the following audio devices:/������� $1 ������������ ��������� �������� ����������:/;
s/The (&man\..*\.[0-9];) driver supports the following PCI sound cards:/������� $1 ������������ ��������� �������� ����� PCI:/;
s/SCSI controllers supported by the (&man\..*\.[0-9];) driver include:/SCSI �����������, �������������� ��������� $1, ��������:/;
s/The (&man\..*\.[0-9];) driver supports the following SATA RAID controllers:/������� $1 ������������ ��������� SATA RAID �����������:/;
s/Devices supported by the (&man\..*\.[0-9];) driver include:/����������, �������������� ��������� $1, ��������:/;
s/The following devices are supported by the (&man\..*\.[0-9];) driver:/��������� ���������� �������������� ��������� $1/;
s/The (&man\..*\.[0-9];) driver supports the following devices:/������� $1 ������������ ��������� ����������:/;
s/The (&man\..*\.[0-9];) driver supports the following parallel to SCSI interfaces:/������� $1 ������������ ��������� parallel to SCSI ����������/;
s/The (&man\..*\.[0-9];) driver supports the following hardware:/������� $1 ������������ ��������� ������������:/;
s/The adapters supported by the (&man\..*\.[0-9];) driver include:/��������, �������������� ��������� $1, ��������:/;
s/The (&man\..*\.[0-9];) driver supports the following Ethernet adapters:/������� $1 ������������ ��������� �������� Ethernet:/;
s/Controllers and cards supported by the (&man\..*\.[0-9];) driver include:/����������� � �����, �������������� ��������� $1, ��������:/;
s/The (&man\..*\.[0-9];) driver supports the following audio chipsets:/������� $1 ������������ ��������� ����� �������:/;
s/The (&man\..*\.[0-9];) driver supports the following sound cards:/������� $1 ������������ ��������� �������� �����:/;
s/The (&man\..*\.[0-9];) driver provides support for the following chips:/������� $1 ������������� ��������� ��� ��������� ���������:/;
if($isOutputFile) {
  next if !/hwlist\.([0-9a-f]+)/;
  print OUTPUTFILE if !$translated{$1};
} else {
print;
}
}
