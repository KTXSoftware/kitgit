var solution = new Solution('kitgit');
var project = new Project('kitgit');

solution.setCmd();

project.addExclude('.git/**');
project.addExclude('libgit2/.git/**');
project.addExclude('build/**');

project.addFile('Sources/**');

function addLibFiles() {
	for (var i = 0; i < arguments.length; ++i) {
		project.addFile('libgit2/' + arguments[i]);
	}
}

addLibFiles('src/*.c', 'src/*.h', 'src/transports/*.c', 'src/transports/*.h', 'src/xdiff/*.c', 'src/xdiff/*.h');

project.addIncludeDirs('libgit2/src', 'libgit2/include');

project.addIncludeDir('libgit2/deps/http-parser');
addLibFiles('deps/http-parser/*.c', 'deps/http-parser/*.h');

if (platform === Platform.Windows) {
	addLibFiles('src/win32/*.c', 'src/win32/*.h');
	project.addDefines('WIN32', '_WIN32_WINNT=0x0501');

	project.addDefine('GIT_WINHTTP');

	project.addDefine('WIN32_SHA1');
	addLibFiles('src/hash/hash_win32.c');

	project.addIncludeDir('libgit2/deps/regex');
	addLibFiles('deps/regex/regex.c');
}
else {
	if (platform === Platform.OSX) {
		project.addDefine('GIT_COMMON_CRYPTO');
	}
	else {
		project.addDefine('OPENSSL_SHA1');
		//project.addLibs('ssl', 'crypto');
	}
	project.addDefine('GIT_SSL');
	//addLibFiles('src/hash/hash_generic.c');
	addLibFiles('src/unix/*.c', 'src/unix/*.h');
}

if (platform === Platform.OSX) {
	project.addLibs('libcrypto.dylib', 'libssl.dylib');
}

project.addIncludeDir('libgit2/deps/zlib');
project.addDefines('NO_VIZ', 'STDC', 'NO_GZIP');
addLibFiles('deps/zlib/*.c', 'deps/zlib/*.h');

solution.addProject(project);

return solution;
