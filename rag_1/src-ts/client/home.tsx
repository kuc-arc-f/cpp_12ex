
import React  from 'react';
import { useState , useEffect }  from 'react';
import { marked } from 'marked';
import Head from '../components/Head'

export default function Chat() {
  const [text, setText] = useState<string>("");
  const [isLoading, setIsLoading] = useState(false);

  useEffect(() => {
  }, []);

  const chatStart = async function(){
    try{    
      setText("");
      setIsLoading(false);
      const elem = document.getElementById("input_text") as HTMLInputElement;
      let inText = "";
      if(elem){
        inText = elem.value;
      };
      console.log("inText=", inText);
      if(!inText){ return; }
      setIsLoading(true);
      const item = {
        input: inText ,
      };
      const newTodo: Todo = {
        query: inText.trim(),
      };
      const sendData = {
        action: "search",
        data: JSON.stringify(newTodo)
      }
      //console.log(newTodo)
      console.log(sendData)
      const sendJson = JSON.stringify(sendData)
      if (window.chrome && window.chrome.webview) {
        const eventHandler = (event) => {
          const resp = event.data;
          console.log("resp=" + resp)
          if(resp){
            const target = JSON.parse(resp)
            console.log(target)
            const targetHtml = marked.parse(target.data);
            setText(targetHtml);
          }
          window.chrome.webview.removeEventListener('message', eventHandler);
          setIsLoading(false);        
        }
        window.chrome.webview.addEventListener('message', eventHandler);
        //post
        window.chrome.webview.postMessage(sendJson);        
      }
      return;
    } catch(e){
      console.error(e);
    }
  }

  return (
  <div className="mb-[200px]">
    <Head />
    <div className="flex flex-col w-full max-w-3xl py-4 mx-auto gap-4">
      <div className="flex flex-col gap-2 px-4 bg-white">
        <h1 className="text-2xl font-bold">RAG-Chat</h1>
        <input
          id="input_text"
          type="text"
          defaultValue=""
          className="w-full p-2 border border-gray-300 rounded dark:disabled:bg-gray-700"
          placeholder="Type your message..."
        />
        <button
          type="button"
          className="px-4 py-2 text-white bg-gray-600 rounded hover:bg-gray-700 disabled:bg-gray-700"
          onClick={()=>{chatStart()}}
        > GO
        </button>

      </div>
      <div>
        <div dangerouslySetInnerHTML={{ __html: text }} id="get_text_wrap"
          className="mb-8 p-2 bg-gray-100" />
        {isLoading ? (
          <div 
          className="animate-spin rounded-full h-8 w-8 mx-4 border-t-4 border-b-4 border-blue-500">
          </div>
        ): null}
      </div>

    </div>
  </div>

  );
}