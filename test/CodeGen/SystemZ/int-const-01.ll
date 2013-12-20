; Test loading of 32-bit constants.
;
; RUN: llc < %s -mtriple=s390x-linux-gnu | FileCheck %s

; Check 0.
define i32 @f1() {
; CHECK: f1:
; CHECK: lhi %r2, 0
; CHECK: br %r14
  ret i32 0
}

; Check the high end of the LHI range.
define i32 @f2() {
; CHECK: f2:
; CHECK: lhi %r2, 32767
; CHECK: br %r14
  ret i32 32767
}

; Check the next value up, which must use LLILL instead.
define i32 @f3() {
; CHECK: f3:
; CHECK: llill %r2, 32768
; CHECK: br %r14
  ret i32 32768
}

; Check the high end of the LLILL range.
define i32 @f4() {
; CHECK: f4:
; CHECK: llill %r2, 65535
; CHECK: br %r14
  ret i32 65535
}

; Check the first useful LLILH value, which is the next one up.
define i32 @f5() {
; CHECK: f5:
; CHECK: llilh %r2, 1
; CHECK: br %r14
  ret i32 65536
}

; Check the first useful IILF value, which is the next one up again.
define i32 @f6() {
; CHECK: f6:
; CHECK: iilf %r2, 65537
; CHECK: br %r14
  ret i32 65537
}

; Check the high end of the LLILH range.
define i32 @f7() {
; CHECK: f7:
; CHECK: llilh %r2, 65535
; CHECK: br %r14
  ret i32 -65536
}

; Check the next value up, which must use IILF.
define i32 @f8() {
; CHECK: f8:
; CHECK: iilf %r2, 4294901761
; CHECK: br %r14
  ret i32 -65535
}

; Check the highest useful IILF value, 0xffff7fff
define i32 @f9() {
; CHECK: f9:
; CHECK: iilf %r2, 4294934527
; CHECK: br %r14
  ret i32 -32769
}

; Check the next value up, which should use LHI.
define i32 @f10() {
; CHECK: f10:
; CHECK: lhi %r2, -32768
; CHECK: br %r14
  ret i32 -32768
}

; Check -1.
define i32 @f11() {
; CHECK: f11:
; CHECK: lhi %r2, -1
; CHECK: br %r14
  ret i32 -1
}
