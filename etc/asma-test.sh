#!/bin/sh
set -e
cd "$(dirname "${0}")/.."
rm -rf asma-test
mkdir asma-test

expect_failure() {
	cat > asma-test/in.tal
	bin/uxncli asma-test/asma.rom asma-test/in.tal asma-test/out.rom 2> asma-test/asma.log
	if ! grep -qF "${1}" asma-test/asma.log; then
		echo "error: asma didn't report error ${1} in faulty code"
		cat asma-test/asma.log
	fi
}

echo 'Assembling asma with uxnasm'
bin/uxnasm projects/software/asma.tal asma-test/asma.rom > asma-test/uxnasm.log
for F in $(find projects -path projects/library -prune -false -or -path projects/assets -prune -false -or -type f -name '*.tal' | sort); do
	echo "Comparing assembly of ${F}"

	UASM_BASE="asma-test/uxnasm-$(basename "${F%.tal}")"
	if ! bin/uxnasm "${F}" "${UASM_BASE}.rom" 2> "${UASM_BASE}.log"; then
		echo "error: uxnasm failed to assemble ${F}"
		cat "${UASM_BASE}.log"
		exit 1
	fi
	xxd "${UASM_BASE}.rom" > "${UASM_BASE}.hex"

	ASMA_BASE="asma-test/asma-$(basename "${F%.tal}")"
	bin/uxncli asma-test/asma.rom "${F}" "${ASMA_BASE}.rom" 2> "${ASMA_BASE}.log"
	if ! grep -qF 'bytes of heap used' "${ASMA_BASE}.log"; then
		echo "error: asma failed to assemble ${F}, while uxnasm succeeded"
		cat "${ASMA_BASE}.log"
		exit 1
	fi
	xxd "${ASMA_BASE}.rom" > "${ASMA_BASE}.hex"

	diff -u "${UASM_BASE}.hex" "${ASMA_BASE}.hex"
done
expect_failure 'Invalid hexadecimal: $defg' <<'EOD'
|1000 $defg
EOD
expect_failure 'Invalid hexadecimal: #defg' <<'EOD'
|1000 #defg
EOD
expect_failure 'Invalid hexadecimal: #' <<'EOD'
|1000 #
EOD
expect_failure 'Invalid hexadecimal: #0' <<'EOD'
|1000 #0
EOD
expect_failure 'Invalid hexadecimal: #000' <<'EOD'
|1000 #000
EOD
expect_failure 'Unrecognised token: 0' <<'EOD'
|1000 0
EOD
expect_failure 'Unrecognised token: 000' <<'EOD'
|1000 000
EOD
expect_failure 'Address not in zero page: .hello' <<'EOD'
|1000 @hello
	.hello
EOD
expect_failure 'Address outside range: ,hello' <<'EOD'
|1000 @hello
|2000 ,hello
EOD
expect_failure 'Unrecognised token: hello' <<'EOD'
hello
EOD
expect_failure 'Macro already exists: %me' <<'EOD'
%me { #00 }
%me { #01 }
EOD
expect_failure 'Memory overwrite: SUB' <<'EOD'
|2000 ADD
|1000 SUB
EOD
expect_failure 'Recursion level too deep:' <<'EOD'
%me { you }
%you { me }
|1000 me
EOD
expect_failure 'Recursion level too deep: ~asma-test/in.tal' <<'EOD'
~asma-test/in.tal
EOD
expect_failure 'Label not found: ;blah' <<'EOD'
|1000 ;blah
EOD
expect_failure 'Label not found: ,blah' <<'EOD'
|1000 ,blah
EOD
expect_failure 'Label not found: .blah' <<'EOD'
|1000 .blah
EOD
echo 'All OK'

