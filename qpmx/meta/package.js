function Component() {}

Component.prototype.createOperations = function()
{
	try {
		component.createOperations();

		if (installer.value("os") === "win") {
			if(installer.value("allUsers") === "true") {
				component.addElevatedOperation("Execute",
											   "cmd", "/c",
											   "xset",  "path", "%PATH%;@TargetDir@", "/M");
			} else {
				component.addOperation("Execute",
									   "cmd", "/c",
									   "xset",  "path", "%PATH%;@TargetDir@");
			}
		} else if (installer.value("os") === "x11") {
			component.addElevatedOperation("CreateLink",
										   "/usr/bin/qpmx",
										   "@TargetDir@/qpmx");
		}
	} catch (e) {
		print(e);
	}
}
