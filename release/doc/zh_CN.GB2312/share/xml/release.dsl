<!-- Original Revision: 131458 -->
<!-- $FreeBSD: stable/9/release/doc/zh_CN.GB2312/share/xml/release.dsl 183116 2008-09-17 18:13:37Z delphij $ -->

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
                (literal "����ĵ����Լ�������FreeBSD���а汾�йص��ĵ����������� ")
		(create-link (list (list "HREF" (entity-text "release.url")))
                  (literal (entity-text "release.url")))
                (literal "���ء�")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "����������FreeBSD�ļ�������ʱ���������Ķ� ")
		(create-link
		  (list (list "HREF" "http://www.FreeBSD.org/docs.html"))
                  (literal "�ĵ�"))
                (literal " ֮���ٿ�����ϵ <")
		(create-link
		  (list (list "HREF" "mailto:questions@FreeBSD.org"))
                  (literal "questions@FreeBSD.org"))
                (literal ">��")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "���� FreeBSD ")
		(literal (entity-text "release.branch"))
		(literal " ���û���Ӧ�ö��� ")
                (literal "<")
		(create-link (list (list "HREF" "mailto:current@FreeBSD.org"))
                  (literal "current@FreeBSD.org"))
                (literal "> �ʼ��б�")))

            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
	      (literal "��������ĵ����κ����⣬������ <")
	      (create-link (list (list "HREF" "mailto:doc@FreeBSD.org"))
                (literal "doc@FreeBSD.org"))
	      (literal ">��")))))
      ]]>

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
