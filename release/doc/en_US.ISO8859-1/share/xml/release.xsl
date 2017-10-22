<?xml version="1.0"?>
<!-- $FreeBSD: release/10.0.0/release/doc/en_US.ISO8859-1/share/xml/release.xsl 260657 2014-01-14 23:58:50Z hrs $ -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                xmlns:db="http://docbook.org/ns/docbook"
                exclude-result-prefixes="db">

  <xsl:param name="release.url"/>
  <xsl:param name="release.branch"/>

  <xsl:template name="user.footer.content">
    <p align="center"><small>This file, and other release-related documents,
      can be downloaded from <a href="{$release.url}"><xsl:value-of select="$release.url"/></a>.</small></p>

    <p align="center"><small>For questions about FreeBSD, read the
      <a href="http://www.FreeBSD.org/docs.html">documentation</a> before
      contacting &lt;<a href="mailto:questions@FreeBSD.org">questions@FreeBSD.org</a>&gt;.</small></p>

    <p align="center"><small>All users of FreeBSD release should
      subscribe to the &lt;<a href="mailto:stable@FreeBSD.org">stable@FreeBSD.org</a>&gt;
      mailing list.</small></p>
  
    <p align="center"><small>For questions about this documentation,
      e-mail &lt;<a href="mailto:doc@FreeBSD.org">doc@FreeBSD.org</a>&gt;.</small></p>
  </xsl:template>
</xsl:stylesheet>
