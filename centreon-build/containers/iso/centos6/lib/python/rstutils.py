# coding: utf-8

def make_title(title, typ='simple', char='='):
    """
    """
    l = len(title)
    if type(title) is unicode:
        title = title.encode("utf-8")
    return "%s%s\n%s\n" % ("%s\n" % (char * l) if typ == "double" else "", title, char * l)
