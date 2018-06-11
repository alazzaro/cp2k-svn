
LIBS += -L$(LIBEXTSDIR)/dbcsr -ldbcsr
dbcsr: 
	$(MAKE) -C $(EXTSHOME)/$@ \
	   INCLUDEMAKE=$(CP2KHOME)/arch/$(ARCH).$(ONEVERSION) \
	   LIBDIR=$(LIBEXTSDIR)/$@ \
	   OBJDIR=$(OBJEXTSDIR)/$@
	@cp $(OBJEXTSDIR)/$@/*.mod $(OBJDIR)/
