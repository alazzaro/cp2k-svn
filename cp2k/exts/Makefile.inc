
LIBS += -L$(LIBEXTSDIR)/dbcsr -ldbcsr
dbcsr: 
	$(MAKE) -C $(EXTSHOME)/$@ \
	   INCLUDEMAKE=$(CP2KHOME)/arch/$(ARCH).$(ONEVERSION) \
	   LIBDIR=$(LIBEXTSDIR)/$@ \
	   OBJDIR=$(OBJEXTSDIR)/$@
	@cp $(OBJEXTSDIR)/$@/dbcsr_api.mod $(OBJDIR)/
	@cp $(OBJEXTSDIR)/$@/dbcsr_tensor_api.mod $(OBJDIR)/
