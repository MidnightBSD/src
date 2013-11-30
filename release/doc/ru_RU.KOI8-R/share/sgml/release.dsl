<!--
  The FreeBSD Russian Documentation Project

  $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/share/sgml/release.dsl,v 1.5 2004/09/13 08:00:12 den Exp $
  $FreeBSD: src/release/doc/ru_RU.KOI8-R/share/sgml/release.dsl,v 1.3 2004/09/13 14:24:46 den Exp $
  
  Original revision: 1.8
-->

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
                (literal "���� ���� � ������ ���������, ����������� �
		������ ������ FreeBSD, ����� ���� ������� �� ������ ")
		(create-link (list (list "HREF" (entity-text "release.url")))
                  (literal (entity-text "release.url")))
                (literal ".")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "���� � ��� ������ ������, ���������� FreeBSD,
		�������� ")
		(create-link
		  (list (list "HREF" "http://www.FreeBSD.org/ru/docs.html"))
                  (literal "������������,"))
                (literal " ������ ��� �������� ������ � <")
		(create-link
		  (list (list "HREF" "mailto:questions@FreeBSD.org"))
                  (literal "questions@FreeBSD.org"))
                (literal ">.")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "���� ������������� FreeBSD ")
		(literal (entity-text "release.branch"))
		(literal " ������������� ����������� �� ������ �������� ")
                (literal "<")
		(create-link (list (list "HREF" "mailto:current@FreeBSD.org"))
                  (literal "current@FreeBSD.org"))
		(literal ">.")))

            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"
	      (literal "�������, ���������� ����� ���������, �� ������
	      ��������� �� ������ <")
	      (create-link (list (list "HREF" "mailto:doc@FreeBSD.org"))
                (literal "doc@FreeBSD.org"))
	      (literal ">.")))))
      ]]>

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
