' html2h translats .html file into C .h file
'
' Bojan Jurca, 12.7.2023


' check arguments

If Wscript.Arguments.Count <> 2 Then
	Wscript.Echo "Synatx error. Use: 'html2h.vbs orgFile.html newFile.h'"
	WScript.Quit
End if

' both arguments exist, check if file arg 1 exists by opening it

Const ForReading = 1, ForWriting = 2 ', ForAppending = 8

inputFileName = Wscript.Arguments (0)
if InStr (inputFileName, ".") = 0 Then inputFileName = inputFileName & ".html" ' assume .html if not specified

Set fso = CreateObject ("Scripting.FileSystemObject")
Set inputFile = fso.OpenTextFile (inputFileName, ForReading) ' the script will end with an error if the file does not exist

	' check if file arg 2 already exists

	outputFileName = Wscript.Arguments (1)
	if InStr (outputFileName, ".") = 0 Then outputFileName = outputFileName & ".h" ' assume .h if not specified

	If fso.FileExists (outputFileName) Then
		If MsgBox (outputFileName & " already exists." & vbCrLf & vbCrLf & "Overwrite?", VbYesNo + vbQuestion, "File already exists") <> vbYes Then
			WScript.Quit
		End If	
	End If

 	If Not fso.FileExists (outputFileName) Then
		Set outputFile = fso.CreateTextFile (outputFileName, True) 
	Else
		Set outputFile = fso.OpenTextFile (outputFileName, ForWriting) ' the script will end with an error if the file can not be created/opened
	End If

	outputFile.Write "// this file has been automatically generated from " & inputFileName & " with html2h.vbs" & vbCrLf & vbCrLf

	outputFile.Write "#define " & Left (outputFileName, InStr (outputFileName, ".") - 1) & " """" \" & vbCrLf
	outputFile.Write "    """" \" & vbCrLf

	' process input file line by line

	Do Until inputFile.AtEndOfStream
		'wscript.echo StrData
    		inputLine = inputFile.ReadLine
		RTrim inputLine
		processLine inputLine, outputFile, 0
	Loop
	outputFile.Write "    """"" & vbCrLf 

	outputFile.Close

inputFile.Close

Wscript.Echo outputFileName & " created."


Sub processLine (inputLine, outputFile, recursionLevel)

	' starting "

	If recursionLevel = 0 Then
		outputFile.Write "    """
	End If


	' a line with reference to picture file? This checking is not prerfectly OK since .png string can also be just a string inside .html file but it will do for now
	p = InStr (inputLine, ".png")
	g = InStr (inputLine, ".gif")
	If g > 0 And g < p Then
		p = g
	End If
	If p > 0 Then ' inputLine contains a reference to picture file - pictures have to be base64 encoded
		' split inputLine to 3 parts: the beginning, the picture reference and the end

		q = 0
		For i = p To Len (inputLine)
			If Mid (inputLine, i, 1) = """" Or Mid (inputLine, i, 1) = "'" Then
				q = i - 1
				Exit For
			End If
		Next
		' q points to the end of picture file name, find the beginning now
		If q > 0 Then
			Do While p > 0
				If Mid (inputLine, p, 1) = """" Or Mid (inputLine, p, 1) = "'" Then
					p = p + 1
					Exit Do
				End If
				p = p - 1
			Loop

			' p points to the beginning of picture file name
			if p > 0 Then

				' 1. recursively process the first part of inputLine
				processLine Left (inputLine, p - 1), outputFile, recursionLevel + 1

				' 2. add base64 encoded picture file here
				processPicture Mid (inputLine, p, q - p + 1), outputFile
				
				' 3. recursively process the last part of inputLine
				processLine Mid (inputLine, q + 1), outputFile, recursionLevel + 1

			End IF
		
		End If

	Else
	
		' a normal line

		For i = 1 To Len (inputLine)
			Select Case Mid (inputLine, i, 1)
				Case vbTab
					outputFile.Write "    " ' change tab to 4 spaces
				Case """"
					outputFile.Write "\""" ' change " to \"
				Case Else
					outputFile.Write Mid (inputLine, i, 1)
			End Select
		Next

	End If


	' ending \n"

	If recursionLevel = 0 Then
		outputFile.Write "\n"" \" & vbCrLf
	End If

End Sub


Sub processPicture (pictureFileName, outputFile)

	' extract only the file name

	Trim pictureFileName
	For i = Len (pictureFileName) to 1 Step -1
		If Mid (pictureFileName, i, 1) = "/" Or Mid (pictureFileName, i, 1) = "\" Then
			pictureFileName = Mid (pictureFileName, i + 1)
			Exit For
		End If
	Next	

	' read the whole picture file into the buffer: https://www.motobit.com/tips/detpg_read-write-binary-files/

	Const adTypeBinary = 1
  	Set BinaryStream = CreateObject ("ADODB.Stream")
  	BinaryStream.Type = adTypeBinary
  	BinaryStream.Open
  	BinaryStream.LoadFromFile pictureFileName ' Load the file data from disk To stream object
  	buffer = BinaryStream.Read ' Open the stream And get binary data from the object

	' base64 encode the buffer: https://stackoverflow.com/questions/70225780/base64-decode-xml-vba

	Set objXML = CreateObject ("MSXML2.DOMDocument")
	Set objNode = objXML.CreateElement ("b64")
	objNode.DataType = "bin.base64"
	objNode.nodeTypedValue = buffer
	base64EncodedBuffer = Replace (objNode.text, vbLf, "") ' compose the base64 encoded picture into only one line of text
	Set objNode = Nothing
	Set objXML = Nothing

	outputFile.Write "data:image/" & Right (pictureFileName, 3) & ";base64," & base64EncodedBuffer
End Sub