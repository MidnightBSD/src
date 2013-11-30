<!-- $FreeBSD: src/release/doc/ja_JP.eucJP/share/sgml/release.dsl,v 1.12 2004/12/29 17:11:02 hrs Exp $ -->
<!-- Original revision: 1.8 -->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY release.dsl PUBLIC "-//FreeBSD//DOCUMENT Release Notes DocBook Language Neutral Stylesheet//EN" CDATA DSSSL>
<!ENTITY % output.html  "IGNORE"> 
<!ENTITY % output.print "IGNORE">
]>

<style-sheet>
  <style-specification use="docbook">
    <style-specification-body>
 
      <![ %output.html; [ 
         <!-- Generate links to HTML man pages -->
        (define %refentry-xref-link% #t)

	(define ($email-footer$)
          (make sequence
	    (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"
                (literal "���Υե������¾����꡼����Ϣ��ʸ��� ")
		(create-link (list (list "HREF" (entity-text "release.url")))
                  (literal (entity-text "release.url")))
                (literal " �����������ɤǤ��ޤ���")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "FreeBSD �˴ؤ��뤪�䤤��碌�ϡ�<")
		(create-link
                  (list (list "HREF" "mailto:questions@FreeBSD.org"))
                  (literal "questions@FreeBSD.org"))
                (literal "> �ؼ������Ƥ�������")
		(create-link
		  (list (list "HREF" "http://www.FreeBSD.org/docs.html"))
                  (literal "����ʸ��"))
                (literal "���ɤߤ���������")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "FreeBSD ")
		(literal (entity-text "release.branch"))
		(literal " �򤪻Ȥ������ϡ����� ")
                (literal "<")
		(create-link (list (list "HREF" "mailto:current@FreeBSD.org"))
                  (literal "current@FreeBSD.org"))
                (literal "> �᡼��󥰥ꥹ�Ȥ˻��ä���������")))

            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
	      (literal "����ʸ��θ�ʸ�˴ؤ��뤪�䤤��碌�� <")
	      (create-link (list (list "HREF" "mailto:doc@FreeBSD.org"))
                (literal "doc@FreeBSD.org"))
	      (literal "> �ޤǡ�")
              (make empty-element gi: "br")
	      (literal "���ܸ����˴ؤ��뤪�䤤��碌�ϡ�<")
	      (create-link (list (list "HREF" "http://www.jp.FreeBSD.org/ml.html#doc-jp"))
                 (literal "doc-jp@jp.FreeBSD.org"))
	      (literal "> �ޤ��Żҥ᡼��Ǥ��ꤤ���ޤ���")))))
      ]]>
    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
