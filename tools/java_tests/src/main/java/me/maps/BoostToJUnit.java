package me.maps;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.DefaultParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.jdom2.Document;
import org.jdom2.input.SAXBuilder;
import org.jdom2.output.XMLOutputter;
import org.jdom2.transform.XSLTransformer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class BoostToJUnit {

  private static final Logger log = LoggerFactory.getLogger(BoostToJUnit.class);

  private String input;
  private String output;
  private String xslt;

  private Document transformed;


  public BoostToJUnit(String input, String output, String xslt) {
    this.input = input;
    this.output = output;
    this.xslt = xslt;

  }


  public static void main(String[] args) {

    Options opts = new Options();
    opts.addOption(Option.builder("i").argName("i").longOpt("input").hasArg().desc("The input file for the transformation, which is the stderr of a boost test").type(String.class).build());
    opts.addOption(Option.builder("t").longOpt("xslt").hasArg().desc("The xslt to use when transforming the log").type(String.class).build());
    opts.addOption(Option.builder("o").longOpt("output").hasArg().desc("The path to the resulting file").type(String.class).build());

    try {
      CommandLine cmd = new DefaultParser().parse(opts, args);
      String in = cmd.getOptionValue("i");
      String out = cmd.getOptionValue('o');
      String xslt = cmd.getOptionValue('t');

      if (not(in) || not(out) || not(xslt)) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        HelpFormatter formatter = new HelpFormatter();
        formatter.printHelp(pw, 80, "ant", "", opts, 2, 10, "If you are running this script from gradle, the options should be prefixed with P, that is -Pi instead of -i");

        log.error(sw.toString());
        return;
      }

      BoostToJUnit b2ju = new BoostToJUnit(in, out, xslt);
      b2ju.transform();
      b2ju.writeToFIle();
    } catch (Exception e) {
      e.printStackTrace();
      log.error("Caught an error", e);
    }
  }


  public void transform() throws Exception {
    XSLTransformer transformer = new XSLTransformer(xslt);
    SAXBuilder builder = new SAXBuilder();
    File xmlFile = new File(input);
    Document untransformed = (Document) builder.build(xmlFile);
    transformed = transformer.transform(untransformed);
  }


  public void writeToFIle() throws Exception {
    File out = new File(output);
    FileWriter fw = new FileWriter(out);
    XMLOutputter outputter = new XMLOutputter();
    outputter.output(transformed, fw);
    fw.flush();
    fw.close();
  }


  private static boolean not(String s) {
    return s == null || s.isEmpty();
  }

}
