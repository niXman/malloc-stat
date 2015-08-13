package hu.qgears.analyzelogmalloc;

import hu.qgears.commons.MultiMapHashImpl;

import java.io.PrintStream;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Processes entries from the log stream by finding allocation/free pairs and summarizing
 * allocated but not freed data.
 * @author rizsi
 *
 */
public class EntryProcessor {
	private Map<String, Entry> allocations = new HashMap<String, Entry>();
	/**
	 * Free entries that correspond to objects that were allocated before reset or when analyser was off.
	 */
	private Map<String, Entry> beforeAllocations = new HashMap<String, Entry>();
	/**
	 * Timestamp of the first entry processed.
	 */
	long tStart=0;
	private long balance;
	private long beforeBalance;
	private int beforeN;
	private int matching;
	private long matchingSum;
	/**
	 * All entries stored currently.
	 * @author rizsi
	 *
	 */
	class Entries
	{
		long sum;
		String key;
		List<Entry> entries;
	}
	public void processOutput(PrintStream out) {
		out.println("Processing timespan in millis (since first log processed after reset, measured with currentTimeMillis): "+formatMem( (System.currentTimeMillis()-tStart)));
		out.println("Allocation balance (bytes, negative means leak): "
				+ formatMem(balance));
		out.println("Number of objects allocated in log session but not freed yet: " + allocations.size());
		out.println("Size of objects freed in log session but not allocated in log session (bytes): "
				+ formatMem(beforeBalance));
		out.println("Number of objects freed in session but not allocated in session: " + beforeN + " without multiple frees: "
				+ beforeAllocations.size());
		out.println("Matching alloc/free pairs through the logging session (n, bytes): " + matching + " "
				+ formatMem(matchingSum));
		MultiMapHashImpl<String, Entry> entriesByAllocator = new MultiMapHashImpl<String, Entry>();
		for (Entry e : allocations.values()) {
			// System.out.println(""+e);
			entriesByAllocator.putSingle(e.getAllocatorKey(), e);
		}
		List<Entries> ess=new ArrayList<EntryProcessor.Entries>();
		for (String key : entriesByAllocator.keySet()) {
			Entries es=new Entries();
			es.key=""+key;
			ess.add(es);
			es.entries = entriesByAllocator.get(key);
			for (Entry e : es.entries) {
				es.sum += e.getSize();
			}
		}
		Collections.sort(ess, new Comparator<Entries>() {
			@Override
			public int compare(Entries o1, Entries o2) {
				return (int)(o2.key.compareTo(o1.key));
			}
		});
		for(Entries e:ess)
		{
			out.println("\nallocator: "+e.key+"\n\tN:" + e.entries.size() + " BYTES: "
				+ formatMem(e.sum)+"\n"+e.entries.get(0).toString());
		}

	}
	private String formatMem(long mem) {
		DecimalFormat formatter = (DecimalFormat) NumberFormat
				.getInstance(Locale.US);
		DecimalFormatSymbols symbols = formatter.getDecimalFormatSymbols();
		symbols.setGroupingSeparator(',');
		formatter.setDecimalFormatSymbols(symbols);
		return formatter.format(mem);
	}

	public void processEntry(Entry e) {
		if(tStart==0)
		{
			tStart=System.currentTimeMillis();
		}
		if (e.isFilled() && !e.isKnown()) {
			System.err.println("unknown entry: " + e);
		}
		if (e.isKnown()) {
			if (e.isAllocation()) {
				allocations.put(e.getAddress(), e);
				balance -= e.getSize();
			}
			if (e.isFree()) {
				Entry before = allocations.remove(e.getAddress());
				if (before != null) {
					balance += e.getSize();
					matching++;
					matchingSum += e.getSize();
				} else {
					Entry prev=beforeAllocations.put(e.getAddress(), e);
					if(prev!=null)
					{
						System.err.println("Memory freed twice: "+e+" "+prev);
					}
					beforeBalance += e.getSize();
					beforeN++;
				}
			}
		}
	}

}
