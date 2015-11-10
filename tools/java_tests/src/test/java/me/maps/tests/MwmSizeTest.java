package me.maps.tests;


import static me.maps.matchers.MwmMapMatcher.matchesMwmSizes;
import static me.maps.utils.Utils.readFilesizesFromCoutriesTxt;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertNotNull;

import java.util.Map;

import org.junit.BeforeClass;
import org.junit.Test;


public class MwmSizeTest {

	public static Map<String, Long> oldMwmSizes;
	public static Map<String, Long> newMwmSizes;


	@BeforeClass
	public static void readMwmSizes() throws Exception {
		newMwmSizes = readFilesizesFromCoutriesTxt("../../data/countries.txt");
		oldMwmSizes = readFilesizesFromCoutriesTxt("../../data/old_countries.txt");
	}


	@Test
	public void test() {
		assertNotNull("Couldn't find countries.txt in the data folder", newMwmSizes);
		assertNotNull("Couldn't find old_countries.txt in the data folder", oldMwmSizes);
		assertThat("Some description in the method call", newMwmSizes, matchesMwmSizes(oldMwmSizes).withinPercent(10.0));
	}
}
