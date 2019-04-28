package exp3;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

public class properties_loader
{
//	public static void main(String[] args) throws IOException
//	{
//		Properties properties = new Properties();
//		
//		// ʹ��InPutStream����ȡproperties�ļ�
//		BufferedReader bufferedReader = new BufferedReader(new FileReader("\\config.properties"));
//		
//		properties.load(bufferedReader);
//		
//		// ��ȡkey��Ӧ��valueֵ
//		properties.getProperty(key);
//	}
	
	public static String get_properties(String filePath, String keyWord)
	{
		Properties prop = new Properties();
		String value = null;
		try 
		{
			// ͨ�����뻺�������ж�ȡ�����ļ�
			InputStream InputStream = new BufferedInputStream(new FileInputStream(new File(filePath)));
			// ����������
			prop.load(InputStream);
			// ���ݹؼ��ֻ�ȡvalueֵ
			value = prop.getProperty(keyWord);
		} 
		catch (Exception e) 
		{
			e.printStackTrace();
		}
		return value;
	}
	
}
