package me.maps.deviceTests;


import static me.maps.matchers.AppIsRunningMatcher.isRunning;
import static org.hamcrest.collection.IsIterableWithSize.iterableWithSize;
import static org.junit.Assert.assertThat;

import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import java.util.Properties;
import java.util.stream.Collectors;

import me.maps.rules.TestLoggerRule;
import me.maps.utils.Adb;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class AppStartsTest {

	private static final String APPNAME = "com.mapswithme.maps.pro";

	private static final Logger log = LoggerFactory.getLogger(AppStartsTest.class);
	private static final String OBB_DIR = "/storage/emulated/legacy/Android/obb/com.mapswithme.maps.pro";
	private static final String APP_FOLDER = "/storage/emulated/legacy/MapsWithMe";

	private static String VERSION_CODE;
	private static String APK;

	
	@Rule
	public TestRule classRule = new TestLoggerRule();

	@BeforeClass
	public static void prepare() throws Exception {
		Properties props = new Properties();
		props.load(new FileInputStream("../../android/gradle.properties"));
		VERSION_CODE = props.getProperty("propVersionCode");
		List<String> apks = apkToInstall("../../android/build/outputs/apk");
		assertThat("There must be only 1 apk to install in our android/build/outputs/apk directory", apks, iterableWithSize(1));
		APK = apks.get(0);
		log.debug(String.format("Apk candidate for installation: %s", APK));
	}


	@Before
	public void before() throws Exception {
		Adb.rmdir(OBB_DIR);
		Adb.rmdir(APP_FOLDER);

		uninstallAllSiblings();
		assertThat("There must be no mapsme apps installed on the device", Adb.packagesGrep(APPNAME), iterableWithSize(0));

		Adb.adbInstall(APK);
		assertThat("There must be 1 mapsme app installed on the device", Adb.packagesGrep(APPNAME), iterableWithSize(1));
	}


	@Test
	public void testAppStartWithoutObbs() throws Exception {
		Adb.startApp();
		assertThat("The app must be running after 10 seconds", APPNAME, isRunning().afterSeconds(10));
	}


	@Test
	public void testAppStartsWithObbs() throws Exception {
		pushObbs();
		Adb.startApp();
		assertThat("The app must be running after 10 seconds", APPNAME, isRunning().afterSeconds(10));
	}


	private void pushObbs() throws Exception {
		Adb.mkdir(OBB_DIR);
		Adb.push("../../android/build/fonts.obb", OBB_DIR, obbName("main", VERSION_CODE));
		Adb.push("../../android/build/worlds.obb", OBB_DIR, obbName("patch", VERSION_CODE));
	}


	private static void uninstallAllSiblings() throws Exception {
		List<String> siblings = Adb.packagesGrep(APPNAME);
		for (String app : siblings) {
			Adb.uninstall(app);
		}
	}


	private String obbName(String type, String version) {
		return String.format("%s.1%s.com.mapswithme.maps.pro.obb", type, version);
	}


	private static List<String> apkToInstall(String dir) throws IOException {
		return Files.list(Paths.get(dir)).filter(s -> s.toString().contains("android-google-universal") && !s.toString().contains("unaligned")).map(s -> s.toString()).collect(Collectors.toList());
	}

}
