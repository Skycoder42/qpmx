
function setBaseArgs(args, qpmxDir, logLevel, colors) {
	args.push("--dir");
	args.push(qpmxDir);
	switch(logLevel) {
	case "quiet":
		args.push("--quiet");
		break;
	case "warn-only":
		args.push("--quiet");
		args.push("--verbose");
		break;
	case "normal":
		break;
	case "verbose":
		args.push("--verbose");
		break;
	}
	if(!colors)
		args.push("--no-color");
	return args;
}
