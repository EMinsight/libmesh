example_name  = reduced_basis_ex5
check_SCRIPTS = run.sh
install_dir   = $(examples_install_path)/reduced_basis/ex5
data          = reduced_basis_ex5.C assembly.h assembly.C rb_classes.h reduced_basis_ex5.in Twisted_Beam.bez.gz run.sh
sources       = $(data)

CLEANFILES = RB_sol.e offline_data/* rb_eval.bin

clean-local:
	-rm -rf offline_data

# also need to link files for VPATH builds
if LIBMESH_VPATH_BUILD
  BUILT_SOURCES = .linkstamp
.linkstamp:
	-rm -f reduced_basis_ex5.in && $(LN_S) -f $(srcdir)/reduced_basis_ex5.in .
	-rm -f Twisted_Beam.bez.gz && $(LN_S) -f $(srcdir)/Twisted_Beam.bez.gz .
	$(AM_V_GEN)touch .linkstamp

  CLEANFILES += reduced_basis_ex5.in .linkstamp
endif

##############################################
# include common example environment
include $(top_srcdir)/examples/Make.common
