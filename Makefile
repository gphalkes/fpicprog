_fake_name $(MAKECMDGOALS):
	make -C src -j4 $(MAKECMDGOALS)