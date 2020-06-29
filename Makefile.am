SUBDIRS            = src man tests
doc_DATA           = README.md LICENSE ChangeLog.md
EXTRA_DIST         = README.md LICENSE ChangeLog.md

## Generate .deb package
package build-deb:
	@dpkg-buildpackage -uc -us -B

## Generate MD5 checksum file
MD5 = md5sum
md5-dist:
	@for file in $(DIST_ARCHIVES); do	\
		$(MD5) $$file > $$file.md5;	\
	done

## Check if tagged in git
release-hook:
	if [ ! `git tag | grep $(PACKAGE_VERSION)` ]; then				\
		echo;									\
		printf "\e[1m\e[41mCannot find release tag $(PACKAGE_VERSION)\e[0m\n";	\
		printf "\e[1m\e[5mDo release anyway?\e[0m "; read yorn;			\
		if [ "$$yorn" != "y" -a "$$yorn" != "Y" ]; then				\
			printf "OK, aborting release.\n";				\
			exit 1;								\
		fi;									\
		echo;									\
	else										\
		echo;									\
		printf "\e[1m\e[42mFound GIT release tag $(PACKAGE_VERSION)\e[0m\n";	\
		printf "\e[1m\e[44m>>Remember to push tags!\e[0m\n";			\
		echo;									\
	fi

## Target to run when building a release
release: distcheck release-hook md5-dist package
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do						\
		printf "%-30s   Distribution tarball\n" $$file;				\
		printf "%-30s   " $$file.md5; cat $$file.md5 | cut -f1 -d' ';		\
		mv $$file* ../;								\
	done
	@for file in `(cd ..; ls $(PACKAGE)_$(VERSION)*)`; do				\
		printf "%-30s   Debian/Ubuntu file\n" $$file;				\
	done
