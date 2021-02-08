import java.util.Iterator;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.multipart.MultipartHttpServletRequest;

@Controller
public class FileUploadController {

    @RequestMapping(value="/upload", method=RequestMethod.POST)
		public String handleFileUpload(MultipartHttpServletRequest request){
			Iterator<String> iterator = request.getFileNames();
			
			while (iterator.hasNext()) {
					String fileName = iterator.next();
					MultipartFile multipartFile = request.getFile(fileName);
					byte[] file = multipartFile.getBytes();
					
					// do stuff...
					
			}
			
			// do stuff...
		
		}
}